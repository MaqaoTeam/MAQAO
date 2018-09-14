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

-- This file contains all constants used in the XLSX module.
-- Constants always are in the Lua table XLSX.consts.<object>.<constant>
-- where <object> is an external object in {XLSX_file, XLSX_sheet, XLSX_table, 
-- XLSX_graph, XLSX_text, colors, borders}
-- or an internal object (font, border, fill, style)


XLSX.consts = {}

--
XLSX.consts.meta = {}
XLSX.consts.meta.name        = "__name"

XLSX.consts.meta.file_type   = "XLSX_file"
XLSX.consts.meta.sheet_type  = "XLSX_sheet"
XLSX.consts.meta.text_type   = "XLSX_text"
XLSX.consts.meta.table_type  = "XLSX_table"
XLSX.consts.meta.graph_type  = "XLSX_graph"
XLSX.consts.meta.comment_type= "XLSX_comment"


-- Constants defining XLSX_file attributes
XLSX.consts.file = {}
XLSX.consts.file.name        = "name"          -- Name of the file
XLSX.consts.file.path        = "path"          -- Path where to create the file
XLSX.consts.file.sheets      = "sheets"        -- A table of XLSX_sheet indexed by id
XLSX.consts.file.extension   = "extension"     -- XLSX extension (".xlsx")
XLSX.consts.file.ustrings    = "ustrings"      -- A table of unique strings (key: string , value: index). To use to get the index of a string
XLSX.consts.file.ostrings    = "ostrings"      -- A table of unique strings (key: index, key: string). To use to iterate with a fix order on strings
XLSX.consts.file.nb_ustrings = "nb_ustrings"   -- Number of unique strings
XLSX.consts.file.nb_strings  = "nb_strings"    -- Number of strings (including repetitions)
XLSX.consts.file.fonts       = "fonts"         -- A table of fonts (key: _hash_font(), value: a font)
XLSX.consts.file.ofonts      = "ofonts"        -- A table of ordered fonts (key: index, value: _hash_font())
XLSX.consts.file.borders     = "borders"       -- A table of borders (key: _hash_border(), value: a border)
XLSX.consts.file.oborders    = "oborders"      --
XLSX.consts.file.fills       = "fills"         -- A table of fills (key: _hash_fill(), value: a fill)
XLSX.consts.file.ofills      = "ofills"        --
XLSX.consts.file.styles      = "styles"        -- A table of styles (key: _hash_style(), value: a style)
XLSX.consts.file.ostyles     = "ostyles"       -- 
XLSX.consts.file.alignments  = "alignments"    -- 
XLSX.consts.file.is_graph    = "is_graph"      -- A boolean set to true if the file has at least one graph
XLSX.consts.file.is_comment  = "is_comment"    -- A boolean set to true if the file has at least one comment


-- Constants defining XLSX_sheet attributes
XLSX.consts.sheet = {}
XLSX.consts.sheet.name       = "name"          --
XLSX.consts.sheet.cells      = "cells"         --
XLSX.consts.sheet.position_x = "x"             --
XLSX.consts.sheet.position_y = "y"             --
XLSX.consts.sheet.ustrings   = "ustrings"      --
XLSX.consts.sheet.ostrings   = "ostrings"      --
XLSX.consts.sheet.nb_ustrings= "nb_ustrings"   --
XLSX.consts.sheet.nb_strings = "nb_strings"    --
XLSX.consts.sheet.column_max = "max_col"       --
XLSX.consts.sheet.line_max   = "max_line"      --
XLSX.consts.sheet.relation_id= "rId"           --
XLSX.consts.sheet.mergedCells= "merged_cells"  --
XLSX.consts.sheet.graphs     = "graphs"        --
XLSX.consts.sheet.colum_size = "col_size"      --
XLSX.consts.sheet.row_size   = "row_size"      --
XLSX.consts.sheet.hyperlinks = "hyperlinks"    --
XLSX.consts.sheet.is_remote_hyperlinks   = "is_remote_hyperlinks"    --
XLSX.consts.sheet.comments   = "comments"      --
XLSX.consts.sheet.comment_id = "comment_id"    --
XLSX.consts.sheet.is_raw_fixed  = "is_raw_fixed"  --


-- Constants defining hyperlinks attributes
XLSX.consts.hyperlink = {}
XLSX.consts.hyperlink.ref    = "ref"           -- String location of the link
XLSX.consts.hyperlink.dst    = "dst"           -- XLSX_sheet targeted by the link
XLSX.consts.hyperlink.dst_cell = "dst_cell"    -- Cell targeted by the link
XLSX.consts.hyperlink.display= "display"       -- Text to display
XLSX.consts.hyperlink.r_file = "r_file"        -- Name of the remote file is link between files
XLSX.consts.hyperlink.r_sheet= "r_sheet"       -- Name of the remote sheet is link between files
XLSX.consts.hyperlink.rel_id = "rel_id"        -- Relation id (used for remote links)


-- Constants defining XLSX_table attributes
XLSX.consts.table = {}
XLSX.consts.table.name       = "name"          -- Table name
XLSX.consts.table.header     = "header"        -- Table header
XLSX.consts.table.data       = "data"          -- Table data
XLSX.consts.table.name_style = "name_style"    -- A table defining the style used for the table name. Use XLSX.consts.text constants
XLSX.consts.table.head_style = "head_style"    -- A table defining the style used for the header. Use XLSX.consts.text constants
XLSX.consts.table.data_style = "data_style"    -- A table defining the style used for data. Use XLSX.consts.text constants
XLSX.consts.table.x_head_loc = "x_header_location"   -- X location of the first header cell
XLSX.consts.table.y_head_loc = "y_header_location"   -- Y location of the first header cell
XLSX.consts.table.orientation= "orientation"   -- Table orientation 


-- Constants to define XLSX.consts.table.orientation
XLSX.consts.table.orientations = {} 
XLSX.consts.table.orientations.columns = "cols" -- First dimension is a set of columns
XLSX.consts.table.orientations.lines = "lines"  -- First dimension is a set of lines 


-- Constants defining XLSX_graph attributes 
XLSX.consts.graph = {} 
XLSX.consts.graph.table = "table" -- XLSX_table associated to the graph 
XLSX.consts.graph.x_top = "x_top" -- X location of the top left cell 
XLSX.consts.graph.y_top = "y_top" -- Y location of the top left cell 
XLSX.consts.graph.size_h_c = "size_height_c" -- Graph height (in cells) 
XLSX.consts.graph.size_w_c = "size_width_c" -- Graph width (in cells) 
XLSX.consts.graph.colors = { -- Colors used in graphs
   "004586", "ff420e", "ffd320", "579d1e", "7e0021", 
   "83caff", "314004", "aecf00", "4b1f6f", "ff950e"
   }
XLSX.consts.graph.custom_colors = "custom_colors"     -- Colors defined by the user
XLSX.consts.graph.y_axis_min = "y_axis_min"    -- Minimal value in the Y axis
XLSX.consts.graph.type       = "type"          -- Type of the graph
-- Constants for graphs used by bar_and_lines type
XLSX.consts.graph.secondary  = "secondary"     -- A list of lines / columns using the secondary scale
XLSX.consts.graph.lines_ser  = "lines_ser"     -- A list of lines / columns represented in line instead of bar
XLSX.consts.graph.primary_name   = "primary_name"     -- Name of the primary scale
XLSX.consts.graph.secondary_name = "secondary_name"   -- Name of the secondary scale
XLSX.consts.graph.grouped        = "grouped"          -- True to group bars, else false (default)

-- Constants to define XLSX.consts.graph.type
XLSX.consts.graph_types = {}
XLSX.consts.graph_types.line           = "line"          --
XLSX.consts.graph_types.bar            = "bar"           --
XLSX.consts.graph_types.bar_and_lines  = "bar_and_lines" --
XLSX.consts.graph_types.pie            = "pie"           --


-- Constants defining XLSX_text attributes
XLSX.consts.text = {}
XLSX.consts.text.value       = "value"         --
XLSX.consts.text.bold        = "bold"          --
XLSX.consts.text.italic      = "italic"        --
XLSX.consts.text.strike      = "strike"        --
XLSX.consts.text.underline   = "underline"     --
XLSX.consts.text.color       = "color"         --
XLSX.consts.text.size        = "size"          --
XLSX.consts.text.background  = "background"    --
XLSX.consts.text.bg_tint     = "bg_tint"       --
XLSX.consts.text.borders     = "borders"       --
XLSX.consts.text.font        = "font"          --
XLSX.consts.text.alignment   = "alignment"     --


-- Constants to define a cell borders
XLSX.consts.borders = {}
XLSX.consts.borders.top      = "top"           -- Define the top border
XLSX.consts.borders.bottom   = "bottom"        -- Define the bottom border
XLSX.consts.borders.right    = "right"         -- Define the right border
XLSX.consts.borders.left     = "left"          -- Define the left border
XLSX.consts.borders.all      = "all"           -- Define all borders at the same time


-- Constants to define border styles
XLSX.consts.borders.none     = "none"          -- No border
XLSX.consts.borders.thin     = "thin"          -- Thin border
XLSX.consts.borders.thick    = "thick"         -- Thick border
XLSX.consts.borders.hair     = "hair"          -- Hair border


-- Constants to define alignment
XLSX.consts.align = {}
XLSX.consts.align.default    = "default"       -- 
XLSX.consts.align.center     = "center"        -- 
XLSX.consts.align.left       = "left"          -- 
XLSX.consts.align.right      = "right"         -- 


-- Internal constants defining font attributes
XLSX.consts.font = {}
XLSX.consts.font.bold        = "b"             -- Is the font is in bold (0 / 1)
XLSX.consts.font.italic      = "i"             -- Is the font is in italic (0 / 1)
XLSX.consts.font.strike      = "strike"        -- Is the font is striked (0 / 1)
XLSX.consts.font.underline   = "u"             -- Is the font is underline (0 / 1)
XLSX.consts.font.color       = "color"         -- The font color
XLSX.consts.font.size        = "sz"            -- The font size
XLSX.consts.font.name        = "name"          -- The font name


-- Internal constants defining border attributes
XLSX.consts.border = {}
XLSX.consts.border.top      = "top"            -- Define the top border
XLSX.consts.border.bottom   = "bottom"         -- Define the bottom border
XLSX.consts.border.right    = "right"          -- Define the right border
XLSX.consts.border.left     = "left"           -- Define the left border
XLSX.consts.border.diagonal = "diagonal"       -- 


-- Internal constants defining fill attributes
XLSX.consts.fill = {}
XLSX.consts.fill.background = "bgColor"        -- Define the background color
XLSX.consts.fill.foreground = "fgColor"        -- Define the foreground color
XLSX.consts.fill.bg_tint    = "bgTint"         -- Define the background tint


-- Internal constants defining style attributes
XLSX.consts.style = {}
XLSX.consts.style.font_id    = "fontId"        -- a font identifier (0 - n)
XLSX.consts.style.border_id  = "borderId"      -- a border identifier (0 - n)
XLSX.consts.style.fill_id    = "fillId"        -- a fill identifier (0 - n)
XLSX.consts.style.id         = "Id"            -- Style identifier (0 - n)
XLSX.consts.style.align      = "align"         -- 


-- Internal constants defining comment attributes
XLSX.consts.comment = {}
XLSX.consts.comment.text        = "text"          --
XLSX.consts.comment.location    = "location"      --
XLSX.consts.comment.x           = "x"             --
XLSX.consts.comment.y           = "y"             --
XLSX.consts.comment.id          = "id"            --
XLSX.consts.comment.color       = "color"         --
XLSX.consts.comment.font_color  = "font_color"    --
XLSX.consts.comment.width       = "width"         --


--
XLSX.consts.size = {}
XLSX.consts.size.col_width       = 3.79            -- default column with in cm. To multiply by 4.28 (why ?) 
XLSX.consts.size.row_height      = 0.61            -- default column height in cm. To convert in pt
XLSX.consts.size.col_width_rate  = 4.28            -- where this value come from ?
XLSX.consts.size.cm_to_pt        = 28.34           --
XLSX.consts.size.cm_to_emu       = 360000          --


