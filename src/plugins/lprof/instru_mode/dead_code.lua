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

-- This file contains dead code (no more reachable), that can be removed
-- Still saved here, in case you would like to revive it (bad idea since not maintained years ago)

-- was in main.lua
function apply_profile(proj,measurement_method,profile,outputp,bin,function_filter_list,model,hwc_parameters)
   local instru_file_text = {text = ""};
   local text;

   text = lprof:load_profile(measurement_method,profile,outputp,bin,function_filter_list,model,hwc_parameters);
   instru_file_text.text = text;
   mil:start(proj,instru_file_text);
end
