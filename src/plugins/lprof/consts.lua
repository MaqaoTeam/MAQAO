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

-- This file defines most of LUA lprof constants, including some string constants (not in strings.lua)
-- Some of them are probably deprecated or no more used...

-- TODO: define lprof.consts instead (by analogy with other modules + directory structure) ?
module("lprof",package.seeall)

--CF AVLTREE.H ENUMERATOR!
INFO_TYPE_FUNC = 1;
INFO_TYPE_LOOP = 2;
INFO_TYPE_EXT_LIB = 3;

-- Index of lua table :
--  * cat_per_node
--  * cat_per_process
--  * cat_per_thread
--  * categorization_summary
INFO_CATEGORY     = 1;
TIME_CATEGORY     = 2
BIN_CATEGORY      = 3;
MPI_CATEGORY      = 4;
OMP_CATEGORY      = 5;
MATH_CATEGORY     = 6;
SYSTEM_CATEGORY   = 7;
PTHREAD_CATEGORY  = 8;
IO_CATEGORY       = 9;
STRING_CATEGORY   = 10;
MEM_CATEGORY      = 11;
OTHERS_CATEGORY   = 12;
TOTAL_CATEGORY    = 13;
NB_CATEGORIES     = 12; -- TOTAL IS NOT A CATEGORY

-- Index of lua table samples_category
SAMPLES_CATEGORY_BIN           = 1;
SAMPLES_CATEGORY_MPI           = 2;
SAMPLES_CATEGORY_OMP           = 3;
SAMPLES_CATEGORY_MATY          = 4;
SAMPLES_CATEGORY_SYSTEM        = 5;
SAMPLES_CATEGORY_PTHREAD       = 6;
SAMPLES_CATEGORY_IO            = 7;
SAMPLES_CATEGORY_STRING        = 8;
SAMPLES_CATEGORY_MEM           = 9;
SAMPLES_CATEGORY_OTHERS        = 10;
SAMPLES_CATEGORY_TOTAL         = 11;
SAMPLES_CATEGORY_NB_CATEGORIES = 10; -- TOTAL IS NOT A CATEGORY


LIBC_STRING_CATEGORY  = 1;
LIBC_IO_CATEGORY      = 2;
LIBC_MEMORY_CATEGORY  = 3;
LIBC_PTHREAD_CATEGORY = 4;
LIBC_OTHER_CATEGORY   = 5;
LIBC_UNKNOWN_FCT      = 6;

-- HWC PERIOD
XSMALL_SAMPLING_PERIOD = 250003;
SMALL_SAMPLING_PERIOD = 500003;
DEFAULT_SAMPLING_PERIOD = 2000003;
LARGE_SAMPLING_PERIOD = 20000003;

-- TIMER PERIOD (MILLISECONDS)
TIMER_XSMALL_SAMPLING_PERIOD  =   2; -- 500 Hz
TIMER_SMALL_SAMPLING_PERIOD   =   5; -- 200 Hz
TIMER_DEFAULT_SAMPLING_PERIOD =  10; -- 100 Hz
TIMER_LARGE_SAMPLING_PERIOD   = 100; --  10 Hz

-- Number max of characters for the "Function Name" colum
FCT_NAME_STR_MAX_SIZE = 50;

-- FOR FUNCTION DISPLAY (d=SFX)
-- RESULTS_ARRAY INDEX MATCHING
FCT_NAME_IDX         = 1;
FCT_MODULE_IDX       = 2;
FCT_SRC_INFO_IDX     = 3;
FCT_TIME_PERCENT_IDX = 4;
FCT_TIME_SECOND_IDX  = 5;
FCT_CPI_RATIO_IDX    = 6;
-- TIMER MODE : No CPI Ratio so module idx is different
TIMER_FCT_MODULE_IDX = 6;

-- FOR LOOP DISPLAY (d=SLX)
-- RESULTS_ARRAY INDEX MATCHING
LOOP_ID_IDX           = 1;
LOOP_MODULE_IDX       = 2;
LOOP_FCT_INFO_NAME_IDX= 3;
LOOP_SRC_LINE_INFO_IDX= 4;
LOOP_LEVEL_IDX        = 5;
LOOP_TIME_PERCENT_IDX = 6;
LOOP_TIME_SECOND_IDX  = 7;
LOOP_CPI_RATIO_IDX    = 8;
-- TIMER MODE : No CPI Ratio so module idx is different
TIMER_LOOP_MODULE_IDX = 8;



--RESUME_ARRAY INDEX MATCHING
-- COMMON
RESUME_HOST_IDX         = 1;
RESUME_PID_IDX          = 2;
RESUME_TID_IDX          = 3;
RESUME_TIME_SECOND_IDX  = 4;
RESUME_TIME_PERCENT_IDX = 5;
--FCT IDX SPECIFICS
RESUME_FCT_NAME_IDX     = 6;
RESUME_FCT_SRC_INFO_IDX = 7;
RESUME_FCT_SAMPLES_IDX  = 8;
RESUME_FCT_MODULE_IDX   = 9;
--LOOP IDX SPECIFICS
RESUME_LOOP_ID            = 6;
RESUME_LOOP_FCT_NAME_IDX  = 7;
RESUME_LOOP_SRC_INFO_IDX = 8;
RESUME_LOOP_LEVEL_IDX     = 9;
RESUME_LOOP_SAMPLES_IDX   = 10;
RESUME_LOOP_MODULE_IDX    = 11;

SUMMARY_FCT_AVERAGE_IDX = 4;
SUMMARY_LOOP_AVERAGE_IDX = 6;

CATEGORIZATION_THREAD   = 1;
CATEGORIZATION_PROCESS  = 2;
CATEGORIZATION_NODE     = 3;
CATEGORIZATION_SUMMARY  = 4;
CATEGORIZATION_FULL     = 5;

BACKTRACE_MODE_CALL     = 1
BACKTRACE_MODE_STACK    = 2
BACKTRACE_MODE_BRANCH   = 3
BACKTRACE_MODE_OFF      = 4

FULL_COLUMN_NAMES_FCT         = "Function Name,Module,Source Info,Time %,Time(s),CPI ratio";
FULL_COLUMN_NAMES_LOOP        = "Loop ID,Module,Function Name,Source Info,Level,Time %,Time (s),CPI ratio";
TIMER_FULL_COLUMN_NAMES_FCT   = "Function Name,Module,Source Info,Time %,Time (s)";
TIMER_FULL_COLUMN_NAMES_LOOP  = "Loop ID,Module,Function Name,Source Info,Level,Time %,Time (s)";
CUSTOM_FULL_COLUMN_NAMES_FCT  = "Function Name,Module,Source Info,";
CUSTOM_FULL_COLUMN_NAMES_LOOP = "Loop ID,Module,Function Name,Source Info,Level,";

SUMMARY_COLUMN_NAMES_FCT_SSV  = "Function Name,Module,Source Info,Time Average (%),Time Min(s) [TID],Time Max(s) [TID],Time Average(s),Samples Min-Max";
SUMMARY_COLUMN_NAMES_FCT      = "Function Name,Module,Source Info,Time Average (%),Time Min(s) [TID],Time Max(s) [TID],Time Average(s)";
SUMMARY_COLUMN_NAMES_LOOP_SSV = "Loop ID,Module,Function Name,Source Info,Level,Time Average (%),Time Min(s) [TID],Time Max(s) [TID],Time Average(s),Samples Min-Max";
SUMMARY_COLUMN_NAMES_LOOP     = "Loop ID,Module,Function Name,Source Info,Level,Time Average (%),Time Min(s) [TID],Time Max(s) [TID],Time Average(s)";

PROCESS_VIEW_HEADERS_FCT = "Function Name,Module,Source Info,Time (%),Time Min (s) [TID],Time Max (s) [TID]"
PROCESS_VIEW_HEADERS_LOOP = "Loop ID,Module,Function Name,Source Info,Level,Time (%),Time Min (s) [TID],Time Max (s) [TID]"

CATEGORIZATION_COLUMN_NAMES   = "Time (s),Binary (%),MPI (%),OMP (%),Math (%),System (%),Pthread (%),IO (%),String (%),Memory (%),Others (%)"

LOOP_TYPE_MAIN = 1;
LOOP_TYPE_PEEL_TAIL = 2;
LOOP_TYPE_PEEL_TAIL_V = 3;

CALLCHAIN_FILTER_UP_TO_BINARY    = 1
CALLCHAIN_FILTER_UP_TO_LIBRARY   = 2
CALLCHAIN_FILTER_UP_TO_SYSTEM    = 3
CALLCHAIN_FILTER_IGNORE_ALL      = 4
CALLCHAIN_DEFAULT_VALUE_FILTER    = 5

output_file_names = {}
output_file_names.functions         = "functions"
output_file_names.loops             = "loops"
output_file_names.functions_process = "functions_per_process"
output_file_names.loops_process     = "loops_per_process"
output_file_names.functions_summary = "functions_summary"
output_file_names.loops_summary     = "loops_summary"
output_file_names.cat_summary       = "cat_summary"
output_file_names.cat_node          = "cat_per_node"
output_file_names.cat_process       = "cat_per_process_from_node"
output_file_names.cat_thread        = "cat_per_thread_from_process"
output_file_names.from_node         = "_from_node"
