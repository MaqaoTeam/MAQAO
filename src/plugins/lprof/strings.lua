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

-- This file contains some string constants
-- If some of them are used only once, could be a good idea to remove them:
-- print ("Start sampling") (in another file) is more readable than:
-- print (MSG_START_SAMPLING) + (MSG_START_SAMPLING = "Start sampling" in this file)
-- and easier to debug...

module("lprof.strings", package.seeall)


--------------------------------------------------------------
--                            LProf                         --
--------------------------------------------------------------
MAQAO_TAG = "[MAQAO]";
MAQAO_LPROF_COMMAND = "maqao lprof"
----------------------------- LProf ---------------------------



--------------------------------------------------------------
--                      LProf Options                       --
--------------------------------------------------------------
OPT_DISPLAY_FUNCTIONS  = "--display-functions"
OPT_DF                 = "-df"

OPT_DISPLAY_LOOPS      = "--display-loops"
OPT_DL                 = "-dl"

OPT_EXPERIMENT_PATH    = "--experiment-path="
OPT_XP                 = "xp="

OPT_DISPLAY_BY_THREADS = "--display-by-threads"
OPT_DT                 = "-dt"

OPT_CATEGORY_VIEW      = "--category-view="
OPT_CV                 = "cv="

OPT_OUTPUT_FORMAT      = "--output-format="
OPT_OF                 = "of="
----------------------- LProf Options ------------------------



--------------------------------------------------------------
--                     Commands Array                       --
--------------------------------------------------------------
COLUMN_LEVEL     = "LEVEL"
COLUMN_REPORT    = "REPORT"
COLUMN_COMMAND   = "COMMAND"

--LEVEL (print_display_commands_array)
LEVEL_FUNCTIONS           = "Functions"
LEVEL_LOOPS               = "Loops"
LEVEL_FCTS_AND_LOOPS      = "Fcts+Loops"

--REPORT (print_display_commands_array)
REPORT_SUMMARY   = "Summary"
REPORT_COMPLETE  = "Complete"
REPORT_HTML      = "HTML"
REPORT_CSV       = "CSV"
----------------------- Commands Array -----------------------



--------------------------------------------------------------
--                         Messages                         --
--------------------------------------------------------------
MSG_STOP_PROFILING         = "STOP PROFILING"
MSG_START_DATA_PROCESSING  = "START DATA PROCESSING"
MSG_DATA_PROCESSING_DONE   = "DATA PROCESSING DONE"
MSG_INFO_XP                = "Your experiment path is "
MSG_INFO_CMD               = "To display your profiling results:"
-------------------------- Messages --------------------------
