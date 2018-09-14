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

-- | Add a global variable
function add_gvar(patch,typ,size,init)
   local m = patch.madras
   return { typ = "gvar"
          , value = m:gvar_new(typ,size,init)
          }
end

-- | Add a global string variable
function add_string_gvar(patch,s)
   return add_gvar(patch,1,-1,s)
end

-- | Add a global int64 variable
function add_int64_gvar(patch, init)
   return add_gvar(patch,0,8,init)
end


-- | Add a TLS variable
-- Set parameter is_init to 1 to force variable being considered initialised 
function add_tls_var(patch,typ,size,init, is_init)
   local m = patch.madras
   return { typ = "tlsvar"
          , value = m:tlsvar_new(typ,size,init, is_init)
          }
end

-- | Add a TLS string variable
function add_tls_string_var(patch,s)
   return add_tls_var(patch,1,-1,s)
end

-- | Add a TLS int64 variable
function add_tls_int64_var(patch, init)
   return add_tls_var(patch,0,8,init)
end
