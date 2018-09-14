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

---	@class Convert Class

Convert = {};

---	Converts a value in the specified base to a decimal one
--	Note : Supported bases are 2 to 16
--	@param val Value to convert
--	@param base Values's base
--	@return Decimal value
function Convert:base2dec(val,base)

	local valstr = nil;	local dec = nil;
	local cur_num = nil;local rbase = nil;
	
	if(type(base) == "number") then rbase = base;
	elseif(type(base) == "string") then rbase = Convert:toonumber(base);
	else print("Error : The base parameter must be either a string or number");return nil;end;
	
	if(base < 2 or base > 16) then print("Error : Allowed bases are 2 to 16");return nil;end;
	
	if(	(type(val) == "string" or type(val) == "number") ) then
		valstr = String:new(val):reverse():totable();
		dec = 0;
		for i=0,valstr:get_size()-1
		do
			if(valstr[i+1] == "A" or valstr[i+1] == "a") then cur_num = 10;
			elseif(valstr[i+1] == "B" or valstr[i+1] == "b") then cur_num = 11;
			elseif(valstr[i+1] == "C" or valstr[i+1] == "c") then cur_num = 12;
			elseif(valstr[i+1] == "D" or valstr[i+1] == "d") then cur_num = 13;
			elseif(valstr[i+1] == "E" or valstr[i+1] == "e") then cur_num = 14;
			elseif(valstr[i+1] == "F" or valstr[i+1] == "f") then cur_num = 15;
			else cur_num = tonumber(valstr[i+1]) end;
			dec = dec + cur_num * math.pow(base,i);
		end
	end
	return dec;
end

---	Converts a decimal value to an hexadecimal one
--	@param dec Decimal value (i.e : 444)
--	@return Hexadecimal value
function Convert:dec2hex(dec)

	local HEX = 16;
	local hexval="",remainder,quotient;

	if(type(dec) ~= "number") then
		return nil;
	end

	quotient = dec;
	while(quotient > 0)
	do
		remainder = math.mod(quotient,HEX);
		if(remainder == 10) then
			hexval = "A"..hexval;
		elseif(remainder == 11) then
			hexval = "B"..hexval;
		elseif(remainder == 12) then
			hexval = "C"..hexval;
		elseif(remainder == 13) then
			hexval = "D"..hexval;
		elseif(remainder == 14) then
			hexval = "E"..hexval;
		elseif(remainder == 15) then
			hexval = "F"..hexval;
		else
			hexval = (tostring(remainder))..hexval;
		end
		quotient = Math:round(quotient/HEX,0);
	end
	
	return hexval;
end

--[[
  - Name : toonumber
  - Description : looks for any number in a string,and
                  converts it to a number whereas the
                  tonumber function would only convert a
                  string containing on only digits
--]]
function Convert:toonumber(string)
	if(type(string) == "string") then
		return tonumber(string.match(string,"%d+"));
	else 
		return nil;
	end
end

---	Decimal to binary (string) conversion
--	@param int Decimal value (i.e : 444)
--	@return binary string value
function Convert:dec2bin(int)
	local str = String:new();
	local nb_elem = 0;
	if(type(int) == "number") then
		if(int == 0) then
			str:set("0");
			return str;
		elseif(int == 1) then
			str:set("1");
			return str;		
		end
		repeat
			str:concat(int % 2);
			int = math.floor(int / 2);
			nb_elem = nb_elem + 1;
		until(int <= 1);
		str:concat("1");		
		return str:reverse();
	else 
		return nil;
	end
end

---	Converts a number to a formated data size string (B => Byte, KB => KiB and MB => MiB)
--	@param int Decimal value (i.e : 444,2048)
--	@return formated data size string (i.e : 444B,2KB)
function Convert:format_data_size(num)
	if(num > 1048576) then
		return Math:round((num/1048576),0).."MB";
	elseif(num > 1024) then
		return Math:round((num/1024),0).."KB";
	else
		return Math:round(num,0).."B";
	end
end
