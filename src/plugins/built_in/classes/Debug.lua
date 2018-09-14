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

---	@class Debug Class

Debug = {};
DebugMeta = {};
DebugMeta.__index  = Debug;
DebugMeta.name	   = "Debug";

Debug.debug_output = false;

---Print debug information
--  @param str String to display
function Debug:__display(str)
  if(Debug.debug_output == true) then
    Message:rawprint(str,2);
  end
end

function Debug:temp(str)
	Debug:__display(str);
end

function Debug:info(str)
	Debug:__display("Info: "..str);
end

function Debug:warn(str)
	Debug:__display("Warning: "..str);
end

function Debug:error(str)
	Debug:__display("Error: "..str);
end

function Debug:critical(str)
	Debug:__display("Error: "..str.." Exiting...");
	os.exit();
end

function Debug:enable(show_advinfo)
  Debug.debug_output = true;
  Message:enable_adv_info();
end

function Debug:is_enabled()
  return Debug.debug_output == true;
end

function Debug:disable()
  Debug.debug_output = false;
end


