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

local calls = { -- Alphabetical order
   ["__errno_location"]    = {},
   ["calloc"]              = {"size_t","size_t"},
   ["close"]               = {"int"},
   ["fork"]                = {},
   ["fprintf"]             = {"FILE *", "char *", "..."},
   ["free"]                = {"void*"},
   ["getenv"]              = {"char*"},
   ["ioctl"]               = {"int", "unsigned long", "..."},
   ["log"]                 = {"double"},
   ["logf"]                = {"float"},
   ["malloc"]              = {"size_t"},
   ["memcpy"]              = {"void *", "void *", "size_t"},
   ["memcmp"]              = {"void *", "void *", "size_t"},
   ["memset"]              = {"void *", "int", "size_t"},
   ["open"]                = {"char *", "int", "int"},
   ["pow"]                 = {"double", "double"},
   ["powf"]                = {"float", "float"},
   ["printf"]              = {"char *", "..."},
   ["rand_r"]              = {"unsigned int"},
   ["rand"]                = {},
   ["read"]                = {"int", "void*", "size_t"},
   ["realloc"]             = {"void*","size_t"},
   ["secure_getenv"]       = {"const char*"},
   ["snprintf"]            = {"void *", "size_t", "char *", "..."},
   ["lc_sprintf"]          = {"void *", "size_t", "char *", "..."},
   ["sqrt"]                = {"double"},
   ["sqrtf"]               = {"float"},
   ["srand"]               = {"unsigned int"},
   ["strcasestr"]          = {"char *", "char*"},
   ["strchr"]              = {"char *", "int"},
   ["strchrnul"]           = {"char *", "int"},
   ["strcpy"]              = {"char *", "char*"},
   ["strrchr"]             = {"char *", "int"},
   ["lc_strncpy"]          = {"char *", "size_t", "char *", "size_t"},
   ["strstr"]              = {"char *", "char*"},
   ["strtol"]              = {"char *", "char **", "int"},
   ["write"]               = {"int", "void *", "size_t"},
}

function is_num_signed(typ)
   return typ == "char" or typ == "short" or typ == "int" or 
      typ == "long" or typ == "long long" or
      typ == "signed char" or typ == "signed short" or typ == "signed int" or
      typ == "signed long" or typ == "signed long long" or
      typ == "int8_t" or typ == "int16_t" or typ == "int32_t" or typ == "int64_t" or
      typ == "intptr_t" or typ == "ssize_t"
end

function is_num_unsigned(typ)
   return typ == "unsigned char" or typ == "unsigned short" or typ == "unsigned int" or
      typ == "unsigned long" or typ == "unsigned long long" or
      typ == "uint8_t" or typ == "uint16_t" or typ == "uint32_t" or typ == "uint64_t" or
      typ == "uintptr_t" or typ == "size_t"
end

function is_pointer(typ)
   return string.sub(typ, #typ) == "*"
end

function is_cstring(typ)
   return typ == "const char*" or typ == "char*" or
      typ == "const char *" or typ == "char *"
end

function call_str(config,asmfile,fct_name,registers)

   local str = fct_name

   local proto = calls[fct_name]
   if not proto then
      return str
   end

   return str

end
