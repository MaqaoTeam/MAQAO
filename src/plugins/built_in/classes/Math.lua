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

---	@class Math Class

Math = {};

function Math:round( num, idp )
	return tonumber( string.format("%."..idp.."f", num ) )
end

--- Returns the base-2 logarithm of x
-- @param x a number (FP)
-- @return log2(x) (FP number)
function Math:log2 (x) return math_log2 (x) end

-- To be consistent with LUA 5.2
bit64 = {};

--- Returns a or b (bitwise or)
-- @param a first number
-- @param b second number
-- @return a or b
function bit64:bor (a, b) return math_or (a, b) end

--- Returns a and b (bitwise and)
-- @param a first number
-- @param b second number
-- @return a and b
function bit64:band (a, b) return math_and (a, b) end

--- Returns a xor b (bitwise xor)
-- @param a first number
-- @param b second number
-- @return a xor b
function bit64:bxor (a, b) return math_xor (a, b) end

--- Returns not a (bitwise not)
-- @param a a number
-- @return not a
function bit64:bnot (a) return math_not (a) end

--- Shift to right by shift bits the binary representation of a number
-- @param shift shifting bits
-- @param a a number
-- @return shifted value
function bit64:rshift (shift, number) return math_rshift (shift, number) end

--- Shift to left by shift bits the binary representation of a number
-- @param shift shifting bits
-- @param a a number
-- @return shifted value
function bit64:lshift (shift, number) return math_lshift (shift, number) end
