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

-- This file defines lprof:load_instru_session() which is used for the instru/trace lprof mode
-- This mode is no more maintained since years, it probably does not work today (2018/02/14)

module("lprof.instru_mode.load",package.seeall)

function lprof:load_instru_session(instru_session)
   if(instru_session ~= nil) then
      local rslts = instru_session.rslt;
      local meta = instru_session.meta;
      local call_functions = {};

      if(rslts ~= nil) then
         Debug:info("Wall cycles = "..rslts.wallcycles);
         --For each thread
         for tid,tinfo in pairs(rslts.threads) do
            local nfid  = rslts.nb_functions;--previous + 1
            call_functions[tid] = {};
            --Create functions for destination functions that are not in the program
            --Aggregate calls' cost at function level
            --For each ifunction
            for ifid,finfo in pairs(tinfo.functions) do
               finfo.calls_cycles = 0;
               --meta.functions:tostring();
               for _,cid in pairs(meta.functions[ifid].calls) do
                  local cinfo  = meta.calls[cid];
                  local ccycles;
                  --print("Reading cid "..cid)
                  if(tinfo.calls[cid] == nil) then
                     print("Call "..cid.." to function "..cinfo.dest_name.." not timed");
                     ccycles = 0;
                     tinfo.calls[cid] = Table:new();
                     tinfo.calls[cid].elapsed_cycles = -1;
                     tinfo.calls[cid].instances = -1;
                  else
                     ccycles = tinfo.calls[cid].elapsed_cycles;
                  end
                  if(call_functions[tid][cinfo.address] == nil) then
                     -- Aggregate calls' cost
                     finfo.calls_cycles = finfo.calls_cycles + ccycles;
                     if(cinfo.known_dest < 1) then
                        --Create function which cycles are defined by the sum of the in edges
                        call_functions[tid][cinfo.address] = {fid = nfid,in_cycles = ccycles,instances = 1};
                        nfid = nfid + 1;
                     end
                  else
                     if(cinfo.known_dest < 1) then
                        --Continue filling information of newly created functions
                        call_functions[tid][cinfo.address].in_cycles =
                           call_functions[tid][cinfo.address].in_cycles + ccycles;
                        call_functions[tid][cinfo.address].instances =
                           call_functions[tid][cinfo.address].instances + 1;
                     end
                  end
               end
            end--For each ifunction
         end--For each thread
         return rslts,meta,call_functions;
      else
         Message:critical("Invalid or missing instrumentation results in result files. Exiting ...");
      end
   else
      Message:critical("No instrumentation results available. Exiting ...");
   end
end
