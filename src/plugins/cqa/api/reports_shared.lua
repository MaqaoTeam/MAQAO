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

--- Module cqa.api.reports_shared
-- Defines the is_intel and is_gnu functions used to check whether a compiler string corresponds to the Intel or the GNU compiler
module ("cqa.api.reports_shared", package.seeall)

local insert = table.insert
local concat = table.concat

--- Checks whether the compiler string is coming from an Intel compiler
-- @param comp_str compiler string ("producer" value in DWARF)
-- @return boolean
function is_intel (comp_str)
   if (comp_str == nil) then return false end

   return string.find (string.lower (comp_str), "intel") ~= nil;
end

--- Checks whether the compiler string is coming from an GNU compiler
-- @param comp_str compiler string ("producer" value in DWARF)
-- @return boolean
function is_gnu (comp_str)
   if (comp_str == nil) then return false end

   return string.find (string.lower (comp_str), "gnu") ~= nil;
end

function get_insn_breakdown_string (insn_table, tags)
   local buf = {};

   local keys = {};
   for k in pairs (insn_table) do insert (keys, k) end
   table.sort (keys);

   for i,insn_name in ipairs (keys) do
      buf[i] = string.format ("%s%s: %d occurrences%s", tags.list_elem_start, insn_name,
                              insn_table [insn_name], tags.list_elem_stop);
   end

   return tags.list_start .. concat (buf) .. tags.list_stop;
end

--- Returns string displaying a min-max value
-- @param x { min = <min value>, max = <max value> }
-- @return "<min value>" if min = max or "<min value>-<max value>" otherwise
function get_min_max_string (x)
   if (x.min == x.max) then
      return string.format ("%.2f", x.min)
   else
      return string.format ("%.2f-%.2f", x.min, x.max)
   end
end

-- @param workarounds table of workarounds (string or structured table)
function get_workaround_string (workarounds, tags)
   if (type (workarounds) ~= "table" or next (workarounds) == nil) then return end

   local function get_tags (list_type, lvl)
      local t = {}

      if (list_type == "ordered") then
         -- ordered list
         t.start = tags.ordered_list_start or tags.list_start
         t.stop  = tags.ordered_list_stop  or tags.list_stop

         if (lvl == 1) then
            t.elem_start = tags.ordered_list_elem_start or tags.list_elem_start
            t.elem_stop  = tags.ordered_list_elem_stop  or tags.list_elem_stop
         else
            t.elem_start = tags.ordered_sublist_elem_start or tags.sublist_elem_start
               or tags.list_elem_start
            t.elem_stop  = tags.ordered_sublist_elem_stop  or tags.sublist_elem_stop
               or tags.list_elem_stop
         end

      else
         -- unordered list
         t.start = tags.list_start
         t.stop  = tags.list_stop 

         if (lvl == 1) then
            t.elem_start = tags.list_elem_start
            t.elem_stop  = tags.list_elem_stop
         else
            t.elem_start = tags.sublist_elem_start or tags.list_elem_start
            t.elem_stop  = tags.sublist_elem_stop  or tags.list_elem_stop
         end
      end

      return t
   end

   local buf = {}

   local function insert_workaround (workaround, lvl)
      if (type (workaround) == "string") then
         insert (buf, workaround)
      else
         if (workaround.list ~= nil) then
            insert (buf, workaround.header .. ":\n")
            local _tags = get_tags (workaround.list_type, lvl)
            insert (buf, _tags.start)
            for i, workaround_item in ipairs (workaround.list) do
               if (workaround.list_type == "ordered") then
                  insert (buf, string.format (_tags.elem_start, i))
               else
                  insert (buf, _tags.elem_start)
               end
               insert_workaround (workaround_item, lvl+1)
               insert (buf, _tags.elem_stop)
            end
            insert (buf, _tags.stop)
         else
            insert (buf, workaround.header)
         end
      end
   end

   -- Only 1 element => don't use list but directly return content
   if (#workarounds == 1) then
      insert_workaround (workarounds[1], 1)
      local ret = concat (buf)

      -- Avoids multiple consecutive \n outside HTML use
      return string.gsub (ret, "\n\n", "\n")
   end

   local _tags = get_tags ("unordered", 1)

   -- More than 1 element => use list
   for _,workaround in ipairs (workarounds) do
      insert (buf, _tags.elem_start)
      insert_workaround (workaround, 2)
      insert (buf, _tags.elem_stop)
   end

   local ret = _tags.start .. concat (buf) .. _tags.stop

   -- Avoids multiple consecutive \n outside HTML use
   return string.gsub (ret, "\n\n", "\n")
end

-- Prints to a string a 2D array
function print_array_to_string (tab, tags, first_line_is_header)
   -- Custom processing of ASCII-art arrays
   local col_width
   local last_elem_start, last_elem_stop
   if (tags.table_start == nil or
       tags.table_start == "") then
      -- For 2-columns arrays, use colon/: separator
      if (#tab[1] == 2) then last_elem_start = ": " end

      -- No trailing tabulation
      last_elem_stop = ""

      -- Padding
      col_width = {}
      for _,line in ipairs (tab) do
         for c, elem in ipairs (line) do
            if (col_width [c] == nil or col_width [c] < #elem) then
               col_width [c] = #elem
            end
         end
      end
   end

   local function fill_line (buf, line, default_elem_start, default_elem_stop)
      for col, v in ipairs (line) do
         local elem_start = default_elem_start
         if (col == #line and last_elem_start) then elem_start = last_elem_start end
         local elem_stop = default_elem_stop
         if ((tags.table_start == nil or tags.table_start == "") and #tab[1] ~= 2) then elem_stop = " | " end
         if (col == #line and last_elem_stop) then elem_stop = last_elem_stop end
         local padd = ""
         if (col < #line and col_width) then padd = string.rep (" ", col_width [col] - #v) end

         buf [col] = elem_start .. v .. padd .. elem_stop
      end
   end

   local lines = {}

   for i, line in ipairs (tab) do
      local buf = {}
      if (i == 1 and first_line_is_header) then
         fill_line (buf, line, tags.table_header_start, tags.table_header_stop)
      else
         fill_line (buf, line, tags.table_elem_start, tags.table_elem_stop)
      end
      lines [i] = tags.table_row_start .. concat (buf) .. tags.table_row_stop
   end

   -- For ASCII-art arrays with an header, inserts a separator line
   if (first_line_is_header and (tags.table_start == nil or tags.table_start == "")) then
      local line_width = 0
      for _,v in ipairs (col_width) do line_width = line_width + v end
      line_width = line_width + #" | " * (#col_width - 1)
      insert (lines, 2, string.rep ("-", line_width) .. "\n")
   end

   collectgarbage ("collect") -- Avoids "out of memory" at table.concat for big arrays

   return tags.table_start .. concat (lines) .. tags.table_stop
end

-- Sometimes, the compiler generates vector instructions for a scalar loop
-- (only the first element in vector registers are used). This is typicaly the case
-- for copying vector registers or taking the absolute value by using a mask.
-- These scalar loops must not be reported as partially vectorized.
-- This function must be called on partially vectorized loops, that is
-- if cqa_results ["packed ratio all"] is a number > 0 and < 100.
-- For these loops that are at first glance partially vectorized, they are not vectorized in reality
-- if these two conditions are met:
-- - no instruction class (except "load" and "other") has a vectorization ratio > 0,
-- - packed ratio is > 0 for the "other" instruction class.
function false_vectorization (cqa_results)
   local packed_ratio_other = cqa_results ["packed ratio"]["other"];
   if (packed_ratio_other == "NA" or packed_ratio_other == 0) then return false end

   for k,v in pairs (cqa_results ["packed ratio"] or {}) do
      if (k ~= "all" and
          k ~= "load" and
          k ~= "other" and
          v ~= "NA" and v > 0) then
         return false
      end
   end

   return true;
end

-- From a list of micro-architectures IDs and an architecture, returns string of "display_names"
function get_uarchs_str (uarch_ids, arch_obj)
   local uarch_names = {}

   for _,uarch_id in ipairs (uarch_ids) do
      if (string.find (uarch_id, "^UARCH_") ~= nil) then
         local uarch = arch_obj:get_uarch_by_id (Consts[arch_obj:get_name()][uarch_id])
         table.insert (uarch_names, uarch:get_display_name())
      else
         table.insert (uarch_names, string.format ("%s (processor)", uarch_id))
      end
   end

   return table.concat (uarch_names, ", ")
end
