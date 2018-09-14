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

-- TODO: much more expensive, should be avoided or called only once...
-- Disabled for the moment
function findLoopBySource(asmfile,file,line)

   if asmfile ~= nil then
      for fct in asmfile:functions() do
         for loop in fct:loops() do
            for block in loop:blocks() do
               for insn in block:instructions() do

                  local p = insn:get_src_file_path()
                  local file2 = string.gsub(p, "/", "")
                  local line2 = ""..insn:get_src_line()
                  if line2 == line and file2 == file 
                  then
                     return loop
                  end
               end
            end
         end
      end
   end

   return nil
end

-- | Parse GCC optimization logs
function gcc_parse(asmfile, logfile)

   local file = io.open(logfile)
   if (file == nil) then return end

   local content = file:read("*all") 

   content = string.gsub(content, "Analyzing loop at ", "[STOP][START]") .. "[STOP]"

   local xps = {}

   for name,line,ct in string.gmatch(content,"%[START%]([%w_%.]*):(%d*)\n(.-)%[STOP%]") do 

      local ct2 = {}
      for nam,l,col,ct3 in string.gmatch(ct, "([%w_%.]*):(%d*):?(%d*): note: (.-)\n") do
         table.insert(ct2, { ["name"]   = nam
                           , ["line"]   = l
                           , ["column"] = col
                           , ["content"] = ct3
                           , ["loop_id"] = nil --findLoopBySource(asmfile, nam, l)
                           })
      end

      table.insert(xps, { ["name"]    = name
                        , ["line"]    = line
                        , ["content"] = ct2
                        , ["loop_id"] = nil --findLoopBySource(asmfile, name, line)
                        })
   end

   return xps
end

-- | Parse ICC optimization logs
function icc_parse(asmfile, logfile)

   local file = io.open(logfile)
   if (file == nil) then return end

   local content = file:read("*all") 

   content = string.gsub(content, "    Report from: ", "[STOP][START]") .. "[STOP]"

   local xps = {}

   for name,ids,ct in string.gmatch(content,"%[START%](.-) %[(.-)%]\n(.-)%[STOP%]") do 
      table.insert(xps, { ["name"] = name
                        , ["ids"]  = ids
                        , ["content"] = ct
                        })
   end

   local xps2 = {} 
   for n,xp in ipairs(xps) do
      local lp = string.match(xp.ids,"loop")
      if lp == nil then
         table.insert(xps2, xp)
      else
         
         local current = { ["name"]    = "root"
                         , ["loops"]   = nil
                         , ["parent"]  = nil
                         , ["content"] = nil
                         }
         for l in string.gmatch(xp.content, "(.-)\r?\n") do
            local path,line,col = string.match(l, "LOOP BEGIN at (.-)%((%d+),(%d+)%)") 
            if path ~= nil then
               local old = current
               current = { ["file"]    = path
                         , ["line"]    = line
                         , ["column"]  = col
                         , ["loops"]   = nil
                         , ["parent"]  = old
                         , ["content"] = nil
                         , ["loop_id"] = nil --findLoopBySource(asmfile, path, line)
                         , ["hasAddr"] = true -- assign address from the debug section
                         }
               current.vectorization               = {}
               current.vectorization.is_vectorized = false
               current.vectorization.is_fused      = false
               current.unrolling                   = {}
               current.unrolling.is_unrolled       = false
               current.unrolling.is_removed        = false
               current.raw_version                 = ""
               current.version                     = "main"
               current.version_number              = 1
            elseif string.match(l, "%s*LOOP END") then
               if current.parent then
                  if current.parent.loops == nil then
                     current.parent.loops = {}
                  end
                  table.insert(current.parent.loops, current)
               end
               current = current.parent
            elseif l ~= "" and string.find (l, "======") == nil then
               local str = string.match(l, "%s*(.*)")
               -- vectorization
               local vec = string.match(str, "remark #%d+: LOOP WAS VECTORIZED")
               if vec ~= nil then
                  current.vectorization.is_vectorized    = true
               end
               local vec = string.match(str, "remark #%d+: REMAINDER LOOP WAS VECTORIZED")
               if vec ~= nil then
                  current.vectorization.is_vectorized    = true
               end
               local fusedvec = string.match(str, "remark #%d+: FUSED LOOP WAS VECTORIZED")
               if fusedvec ~= nil then
                  current.vectorization.is_vectorized    = true
                  current.vectorization.is_fused         = true
               end
               local notvec = string.match(str, "remark #%d+: loop was not vectorized: (.*)")
               if notvec ~= nil then
                  current.vectorization.reason = notvec
                  current.hasAddr = false
               end
               if current.vectorization and current.vectorization.is_vectorized then
                  current.version = "vectorized " .. current.version
               end
               -- Unrolling
               local unroll_factor = string.match(str, "remark #%d+: completely unrolled by (%d+)")
               local unroll_removed = false
               if unroll_factor == nil then
                  unroll_factor = string.match(str, "remark #%d+: unrolled with remainder by (%d+)")
               else
                  unroll_removed = true
               end
               if unroll_factor ~= nil then
                  current.unrolling.is_unrolled   = true
                  current.unrolling.factor        = unroll_factor
                  current.unrolling.is_removed    = unroll_removed
                  if current.raw_version ~= nil then
                     local rem = string.match(current.raw_version, "Remainder")
                     if rem ~= nil then
                        current.unrolling.version = "tail"
                     else
                        current.unrolling.version = "main"
                     end
                  end
               end
               -- Version
               local ver = string.match(str, "<(.+)>")
               if ver ~= nil then
                  current.raw_version = ver
                  if string.match(current.raw_version, "Remainder") ~= nil then
                     current.version = "tail"
                  end
                  if string.match(current.raw_version, "Peeled") ~= nil then
                     current.version = "peel"
                  end
                  local v = string.match(current.raw_version, "Multiversioned v(%d+)")
                  if v ~= nil then
                     current.version_number = v
                  end
               end

               -- raw contents
               if str ~= "===========================================================================" then
                  if current.content == nil then
                     current.content = {}
                  end
                  table.insert(current.content, str)
               end
            end
         end
         
         xp.content = current
         table.insert(xps2, xp)
      end
   end

   return xps2
end
