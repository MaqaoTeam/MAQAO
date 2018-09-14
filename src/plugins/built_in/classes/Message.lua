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

---	@class Message Class

Message = {};
MessageMeta = {};
MessageMeta.__index  = Message;
MessageMeta.name     = "Message";

Message.adv_output = false;
Message.msg_output = true;
Message.exit_mode  = "os";

---Print a message
--  @param str String to display
function Message:__display(str)
   if(type(str) == "string" and Message.msg_output == true) then
      print(str);
   end
end

function Message:rawprint(str,add_call_depth)
   if(Message.adv_output == true) then
      local debug_info = Message:get_env_info(add_call_depth);
      str = debug_info..str;
   end
   Message:__display(str);
end

function Message:temp(str, verbose_lvl, msg_lvl)
	Message:__display("Temp: "..str, verbose_lvl, msg_lvl);
end

--- Displays a message without prefix - equivalent to C macro STDMSG
--  @param str String to display
--  @param verbose_lvl Current verbosity level of the application (optional)
--  @param msg_lvl Minimal verbose level for which the message must be displayed (optional)
function Message:print(str, verbose_lvl, msg_lvl)
   if (verbose_lvl == nil or msg_lvl == nil or msg_lvl <= verbose_lvl) then
      Message:__display(str, verbose_lvl, msg_lvl);
   end
end

function Message:info(str, verbose_lvl, msg_lvl)
	Message:print("Info: "..str, verbose_lvl, msg_lvl);
end

function Message:warn(str)
   if maqao_isatty (io.stdout) == 1 then
     Message:__display("\n\27[33m\27[1m* Warning:\27[21m "..str.."\27[0m");
   else
     Message:__display("\n* Warning: "..str);
   end
end

function Message:error(str)
   if maqao_isatty (io.stdout) == 1 then
      Message:__display("\n\27[31m\27[1m** Error:\27[21m "..str.."\27[0m");
   else
      Message:__display("\n**Error: "..str);
   end
end

function Message:critical(str,exit_code,mod)
   if maqao_isatty (io.stdout) == 1 then
      Message:__display("\n\27[31m\27[1m*** Critical: "..str.."\27[0m");
   else
      Message:__display("\n*** Critical: "..str);
   end

	if(type(exit_code) == "number") then
     if(Message.exit_mode == "os") then
		    os.exit(exit_code);
     else
        error(exit_code,0);--0 means no prefix/suffix text
     end
	else
     if(Message.exit_mode == "os") then
        os.exit(-1);
     else
        error(-1,0);--0 means no prefix/suffix text
     end
	end
end

function Message:display(obj,...)
  if(type(obj) == "table") then
     local num = obj.num;
     local str = obj.str;
     local mod = obj.mod;
     local typ = obj.typ;
     local msg = str;
     
     if(select ("#", ...) > 0) then
        msg = string.format(str, ...);
     end

     num = errcode.buildcode(mod, typ, num);
     
     if(typ == Consts.errors.ERRLVL_CRI) then
        Message:critical(msg,num,mod);
     elseif(typ == Consts.errors.ERRLVL_ERR) then
        Message:error(msg,num,mod);
     elseif(typ == Consts.errors.ERRLVL_WRN) then
        Message:warn(msg,num,mod);
     elseif(typ == Consts.errors.ERRLVL_NFO) then
        Message:info(msg,num,mod);
     end
  else
     Message:critical("Invalid message object",-1,"Message");
  end
end

function Message:get_env_info(add_call_depth)
	local debug_info,debug_txt;
  local depth = 4;	
	-- there are 3 levels of calls between Message:print and the 
	-- current function. 4th (3+1) level is our real target.
	debug_txt  = "";
  if(type(add_call_depth) == "number") then
     depth = depth + add_call_depth;
  end
	debug_info = debug.getinfo(depth);

	if(type(debug_info) == "table") then
		if(debug_info.name ~= nil) then
			debug_txt = debug_txt.."In fct "..debug_info.name;
		end	
		if(debug_info.currentline ~= nil and debug_info.source ~= nil) then
			local lua_file = debug_info.source;

			if(lua_file == nil) then
				lua_file = "";
			end
			debug_txt = debug_txt.." ("..lua_file.." L"..debug_info.currentline..")";
		end
		debug_txt = debug_txt..":\n";
	end
	
	return debug_txt;
end

function Message:enable_adv_info()
	Message.adv_output = true;
end

function Message:disable_adv_info()
	Message.adv_output = false;
end

function Message:enable()
  Message.msg_output = true;
end

function Message:disable()
  Message.msg_output = false;
end

function Message:set_exit_mode(mode)
  if(mode == "os" or mode == "lib") then
    Message.exit_mode = mode;
  else
    Message:__display("Error: Trying to set an Invalid exit mode. Valid modes are: os or lib.");
  end
end




