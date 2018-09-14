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

--- Module cqa.consts
-- Defines the cqa-specific constants
module ("cqa.consts", package.seeall)

FRONT_END_ANALYSIS_ENABLED = 1 -- 0 to disable front-end analysis. In that case...
-- only macro-fusion is done and uops are supposed to come from the uops queue...
-- The effect is similar to setting this variable to 1 with a loop that fits into...
-- the uop cache of a Sandy Bridge processor (with no bottleneck in ROB-read).
LOOP_MERGE_DISTANCE = 2 -- related to source loops merging (src_loop_grouping.lua)
MAX_NB_PATHS = 8 -- maximum number of paths allowed to analyze a loop

local mod_id = Consts.errors.MODULE_CQA;

Errors = {

   MISSING_MANDATORY_OPTIONS = {
      str = "Usage: maqao cqa <binary> [fct-loops=<functions> | fct-body=<functions> | loop=<loops> | path=<block IDs>] [...]",
      num = 1,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   MORE_THAN_ONE_OBJECT_TYPE = {
      str = "fct, fct-loops, loop and path options cannot be used simultaneously",
      num = 2,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   CANNOT_OPEN_BINARY = {
      str = "Cannot open binary: %s",
      num = 3,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   INVALID_INSTRUCTION_MODIFIER = {
      str = "Invalid instructions modifier. Valid: %s",
      num = 4,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   INVALID_OUTPUT_FORMAT = {
      str = "%s is not a valid output format\nValid formats: txt, csv, html",
      num = 5,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   UNEXPECTED_CSV_OUTPUT_PATH = {
      str = "Cannot output text or HTML into a CSV file",
      num = 6,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   INVALID_CONFIDENCE_LEVEL = {
      str = "Invalid confidence level: %s. Valid: gain,potential,hint,expert",
      num = 7,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   CANNOT_OPEN_BLOCKS_SELECTION_FILE = {
      str = "File not found: user.lua",
      num = 8,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   CANNOT_LOAD_BINARY = {
      str = "Cannot disassemble the binary %s",
      num = 9,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   UNSUPPORTED_UARCH = {
      str = "%s is not supported by CQA\nSupported micro architectures: %s",
      num = 10,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   INSTRUCTION_MODIFICATION_FAILURE = {
      str = "Failed to modify some instructions. Rerun with -dbg for more details",
      num = 11,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   CANNOT_WRITE_CSV = {
      str = "Cannot write into %s%s%s.csv",
      num = 12,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   CANNOT_WRITE_HTML = {
      str = "Cannot write %s/index.html",
      num = 13,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   CANNOT_MKDIR_JS = {
      str = "Cannot create directory for JS files: %s",
      num = 14,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   CANNOT_MKDIR_CSS = {
      str = "Cannot create directory for CSS files: %s",
      num = 15,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   CANNOT_MKDIR_IMAGES = {
      str = "Cannot create directory for HTML images: %s",
      num = 16,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   CANNOT_MKDIR_CSS_IMAGES = {
      str = "Cannot create directory for CSS images: %s",
      num = 17,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   MISSING_UARCH = {
      str = "You must specify a micro-architecture\nValid micro architectures: %s",
      num = 18,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   CANNOT_OPEN_UARCH_MODEL_FILE = {
     str = "File not found: %s",
     num = 19,
     mod = mod_id,
     typ = Consts.errors.ERRLVL_CRI
   },
   INVALID_CQA_TAB_INVALID_INSN_ENTRY = {
      str = "Invalid microbench table format (corrupted or incomplete):\n"..
	 "Invalid [%s] instruction entry: must be a table (keys: 32, 64... (memory operand sizes))",
      num = 20,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   INVALID_CQA_TAB_INVALID_SIZE_ENTRY = {
      str = "Invalid microbench table format (corrupted or incomplete):\n"..
	 "Invalid [%s][%s] size entry: must be a table (keys: src and/or dst)",
      num = 21,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   INVALID_CQA_TAB_INVALID_LEVELS_ENTRY = {
      str = "Invalid microbench table format (corrupted or incomplete):\n"..
	 "Invalid or missing %s levels entry: must be a table (keys: L1, L2, L3, RAM)",
      num = 22,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   INVALID_CQA_TAB_INVALID_RAM_ENTRY = {
      str = "Invalid microbench table format (corrupted or incomplete):\n"..
	 "Invalid or missing %s[RAM] entry: must be a table (keys: at least stride1)",
      num = 23,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   INVALID_CQA_TAB_INVALID_MIN_MAX_ENTRY = {
      str = "Invalid microbench table format (corrupted or incomplete):\n"..
      "Invalid or missing %s entry. Expected: { min = <number>, max = <number> }",
      num = 24,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   CANNOT_OPEN_USER_DATA_FILE = {
      str = "Cannot read user data table %s: %s",
      num = 25,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   INVALID_USER_DATA = {
      str = "Invalid user-data table format (corrupted or incomplete):\n%s",
      num = 26,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   CANNOT_OPEN_INSN_EXT_BYPASS_FILE = {
      str = "File not found: %s",
      num = 27,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   CANNOT_OPEN_INSN_CQA_TAB_FILE = {
      str = "Cannot read cqa_tab file containing instructions cycles %s: %s",
      num = 28,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   CANNOT_OPEN_PATTERN_CQA_TAB_FILE = {
      str = "Cannot read cqa_tab file containing pattern cycles %s: %s",
      num = 29,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_NFO
   },
   CANNOT_FIND_INSN_CQA_TAB = {
      str = "Cannot find a table named __ubench_cycles in %s",
      num = 30,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   CANNOT_FIND_PATTERN_CQA_TAB = {
      str = "Cannot find a table named __ubench_cycles_pattern in %s",
      num = 31,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   CANNOT_READ_MEMORY_LEVELS_LIST = {
      str = "Cannot read memory level list (table named __ubench_cycles_memory_levels).\n"..
         "Will be deduced from first __ubench_cycles entry",
      num = 32,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_WRN
   },
   MISSING_ARCH = {
      str = "You must specify an architecture with arch=<arch>\nValid architectures: %s",
      num = 33,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   INVALID_ARCH = {
      str = "You specified an invalid/unsupported architecture\nValid architectures: %s",
      num = 34,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
   UNSUPPORTED_PROC_UARCH = {
      str = "You specified a processor from a micro architecture not supported by CQA: %s\nSupported micro architectures: %s",
      num = 35,
      mod = mod_id,
      typ = Consts.errors.ERRLVL_CRI
   },
}

-- Micro-architectures potentially excluded in CQA (in addition to ones excluded from MAQAO core)
potentially_excluded_uarchs = {} -- Example: { "KABY_LAKE", "KNIGHTS_LANDING" }
