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

--- Module cqa.output.csv
-- Defines objects needed for CSV default output provided by the CQA tool
module ("cqa.output.csv", package.seeall)

-- Metrics to display, in this order, at the beginning of default CSV files
CSV = { "function name", "addr", "src file", "src line min-max",
        "id", "can be analyzed", "nb paths",
        "unroll info", "unroll confidence level",
        "is main/unrolled", "unroll factor", "path ID", "nb instructions" }

-- Metrics to display, in this order, into standard CSV format, after CSV set
CSV_arch_dep = {
}
