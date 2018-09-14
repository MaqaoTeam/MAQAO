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

--- Module cqa.arm64.general_properties.uops
-- Defines functions related to the front-end of the processor and, to be more precise,
-- corresponds to the in-order stages from instruction fetch to register read.
module ("cqa.api.arm64.general_properties.uops", package.seeall)

--- csv column's name
local INSTRUCTION_NAME        = "Instruction";
local UOPS                    = "Uops";
local THROUGHPUT              = "Throughput";
local LATENCY                 = "Latency";
-- Constraints columns
local OPERAND                 = "Operand";
local OPERAND_MODIFIER        = "Modifier";
local INSTRUCTION_VECTOR      = "Flags";
local INSTRUCTION_EXTENSION   = "Extension";
local FIRST_INSN_VARIANT      = "first";
local LAST_INSN_VARIANT       = "last";

--- Update the uops array with the uops of an instruction
function update_uops_count (uops_array_min, uops_array_max, insn, process_uops_multi_units, uop_indexes)

   local insn_info = insn:get_dispatch();

   if (insn_info == nil) then
      print("Warning: could not get extensions for the following instruction");
      print(insn);
      return false, nil;
   end

   local throughput_min = insn_info["throughput"]["min"];
   local throughput_max = insn_info["throughput"]["max"];
   local uops_dispatch = insn_info["dispatch"];
   local multi_ports = {};

   -- Avoid division by 0, sets a 100 cycles cost
   if (throughput_min == 0) then
      throughput_min = 0.01;
   end
   if (throughput_max == 0) then
      throughput_max = 0.01;
   end

   if (process_uops_multi_units == true) then
      
      for _, uop_index in pairs(uop_indexes) do
         local uop_dispatch = uops_dispatch[uop_index];
         local available_ports = {};

         local nb_available_ports = 0;
         for port, available in pairs(uop_dispatch) do

            -- Count available ports for the uop
            if (available == 1) then
               nb_available_ports = nb_available_ports + 1;
               table.insert(available_ports, port);
            end
         end

         -- WARNING: arguable heuristic
         -- Assuming first uop is the one limiting throughput
         -- Other uops assume a throughput of 1 * available units
         if (uop_index > 1) then
            throughput_min = nb_available_ports;
            throughput_max = nb_available_ports;
         end

         local max_uops = 0;
         local port_with_max_uops = "none";
         local exclude_unit = false;
         -- WARNING: handle only 2 destinations
         for _, port in pairs(available_ports) do

            if (uops_array_max[port] > max_uops) then
               max_uops = uops_array_max[port];
               if (port_with_max_uops ~= "none") then
                  exclude_unit = true;
               end
               port_with_max_uops = port;
            elseif (uops_array_max[port] == max_uops) then
               exclude_unit = false;
            end
         end

         if (exclude_unit == true) then
            nb_available_ports = nb_available_ports - 1;
            throughput_min = throughput_min / 2.0;
            throughput_max = throughput_max / 2.0;
         end

         for _, port in pairs(available_ports) do
            if (exclude_unit == false or port ~= port_with_max_uops) then
               uops_array_max[port] = uops_array_max[port] + (nb_available_ports/throughput_min)/nb_available_ports;
               uops_array_min[port] = uops_array_min[port] + (nb_available_ports/throughput_max)/nb_available_ports;
            end
         end
      end

   else

      for i, uop_dispatch in ipairs(uops_dispatch) do

         local nb_available_ports = 0;
         local target_port = nil;
         for port, available in pairs(uop_dispatch) do

            -- Count available ports for the uop
            if (available == 1) then
               nb_available_ports = nb_available_ports + 1;
               target_port = port;
            end

            if (nb_available_ports > 1) then
               target_port = nil;  
            end
            
         end

         if (target_port ~= nil) then
            uops_array_max[target_port] = uops_array_max[target_port] + (1/throughput_min);  
            uops_array_min[target_port] = uops_array_min[target_port] + (1/throughput_max);
         else
            table.insert(multi_ports, i);
         end
      end
   end

   --print(insn)
   --for k,v in pairs(uops_array_max) do
      --print(k..": "..v);
   --end

   if (#multi_ports == 0) then
      return true;
   else
      return false, multi_ports;
   end

end

--- Returns the micro-operations (uops) produced by a sequence of instructions
--- This function should set the following entries:
---    - "[arm64] uops min"
---    - "[arm64] uops max"
-- @param cqa_env
function get_characteristics (cqa_env)
   local insns = cqa_env.insns;

   -- Should be micro architecture specific !
   cqa_env["[arm64] uops min"] = { ["B"] = 0.0, ["I0"] = 0.0, ["I1"] = 0.0, ["M"] = 0.0, ["L"] = 0.0, ["S"] = 0.0, ["F0"] = 0.0, ["F1"] = 0.0 };
   cqa_env["[arm64] uops max"] = { ["B"] = 0.0, ["I0"] = 0.0, ["I1"] = 0.0, ["M"] = 0.0, ["L"] = 0.0, ["S"] = 0.0, ["F0"] = 0.0, ["F1"] = 0.0 };

   local uops_multi_units = {};

   for _,insn in pairs(insns) do

      local uop_to_parse_later;
      local parsed, uop_to_parse_later = update_uops_count(cqa_env["[arm64] uops min"], cqa_env["[arm64] uops max"], insn, false, nil);

      if (parsed == false) then
         table.insert(uops_multi_units, {insn, uop_to_parse_later});
      end

   end

   for _,info in pairs(uops_multi_units) do
      update_uops_count(cqa_env["[arm64] uops min"], cqa_env["[arm64] uops max"], info[1], true, info[2]);
   end
end

--- Update the uops array with the uops of an instruction
-- function update_uops_count (uops_array_min, uops_array_max, insn_uops_info, insn_throughput_info, process_uops_multi_units)
--    -- Parse info for the matched instruction
--    for field in string.gmatch(insn_uops_info, "([^+]+)+?") do
--       local throughput_min = insn_throughput_info;
--       local throughput_max = insn_throughput_info;
            
--       -- Set throughput min and maximum
--       if (string.find(throughput_min, "-")) then
--          throughput_min,_ = string.gsub(throughput_min, "([%p%d]+)-([%p%d]+)", "%1");
--       end
--       if (string.find(throughput_max, "-")) then
--          throughput_max,_ = string.gsub(throughput_max, "([%p%d]+)-([%p%d]+)", "%2");
--       end

--       throughput_min = tonumber(throughput_min, 10);
--       throughput_max = tonumber(throughput_max, 10);
--       --print(throughput_min.." - "..throughput_max);

--       -- Avoid division by 0, sets a 100 cycles cost
--       if (throughput_min == 0) then
--          throughput_min = 0.01;
--       end
--       if (throughput_max == 0) then
--          throughput_max = 0.01;
--       end

--       -- Normalize cost with the number of execution unit available
--       -- (When there is a '/', it means that the uop can go to two differents units, we add 0,5 to each unit)
--       if (string.find(field, "/")) then
--          if (process_uops_multi_units == true) then
--             local max_uops = 0; local nb_ports = 0;
--             local unit_with_max_uops = "none";
--             local exclude_unit = false;

--             _,nb_ports = string.gsub(field,"/","");
--             nb_ports = nb_ports + 1;

--             -- We check if a unit is more used than others
--             for port in string.gmatch(field, "([^/]+)/?") do
--                --print("Checking port: "..port.." -> "..uops_array_max[port]);
--                if (uops_array_max[port] > max_uops) then
--                   max_uops = uops_array_max[port];
--                   if (unit_with_max_uops ~= "none") then
--                      exclude_unit = true;
--                   end
--                   unit_with_max_uops = port;
--                elseif (uops_array_max[port] < max_uops) then
--                   if (unit_with_max_uops ~= "none") then                                                             
--                      exclude_unit = true;                                                                            
--                   end
--                end
--             end
--             if (exclude_unit == true) then
--                nb_ports = nb_ports - 1;
--                throughput_min = throughput_min - 1;
--                throughput_max = throughput_max - 1;
--             end

--             for port in string.gmatch(field, "([^/]+)/?") do
--                if (exclude_unit == false or port ~= unit_with_max_uops) then
--                   uops_array_max[port] = uops_array_max[port] + (nb_ports/throughput_min)/nb_ports;
--                   uops_array_min[port] = uops_array_min[port] + (nb_ports/throughput_max)/nb_ports;
--                end
--             end
--          else
--             --print("To be parsed later: "..field);
--             return false, field; 
--          end
--       else
--          uops_array_max[field] = uops_array_max[field] + (1/throughput_min);
--          uops_array_min[field] = uops_array_min[field] + (1/throughput_max);
--       end
--    end
--    return true;
-- end

-- --- Returns the micro-operations (uops) produced by a sequence of instructions, annotate the DDG with the correct latencies and get the RecMII of theses instructions
-- --- This function should set the following entries:
-- ---    - "[arm64] uops min"
-- ---    - "[arm64] uops max"
-- ---    - "[arm64] latencies"
-- -- @param cqa_env
-- function get_characteristics (cqa_env)
--    local insns = cqa_env.insns;
   
--    -- TODO change the way we get instructions characteristics
--    local info_file_path    = "/home/hbollore/dev/MAQAO/src/plugins/cqa/arm64/Arm64_Insn_Uops.csv"
--    local info_file         = io.open(info_file_path);
--    local info_columns      = {};
--    local insn_variants     = {};
--    local info              = {};

--    -- Read column's name
--    local headers = info_file:read("*l");
--    for name in string.gmatch(headers,"([^;]+)") do
--       table.insert(info_columns, name);
--    end

--    -- Parse instructions' info
--    local line_number      = 1;
--    local previous_insn    = "";
--    for line in info_file:lines("*L") do
--       local index           = 1;
--       info[line_number]     = {};

--       -- Parse fields for each instruction variant
--       for field in string.gmatch(line, "([^;]*);?") do
--          if (index > #info_columns) then 
--             break; 
--          end

--          info[line_number][info_columns[index]] = field;
--          index = index + 1;
--       end

--       -- Build an array listing the lines corresponding to the variants of an instruction
--       if (info[line_number][INSTRUCTION_NAME] ~= previous_insn) then
--          insn_variants[info[line_number][INSTRUCTION_NAME]] = {};
--          insn_variants[info[line_number][INSTRUCTION_NAME]][FIRST_INSN_VARIANT] = line_number;

--          if (previous_insn ~= "") then
--             insn_variants[previous_insn][LAST_INSN_VARIANT] = (line_number - 1);
--          end

--          previous_insn = info[line_number][INSTRUCTION_NAME];
--       end

--       line_number = line_number + 1;
--    end

--    -- Should be micro architecture specific !
--    cqa_env["[arm64] uops min"] = { ["B"] = 0.0, ["I0"] = 0.0, ["I1"] = 0.0, ["M"] = 0.0, ["L"] = 0.0, ["S"] = 0.0, ["F0"] = 0.0, ["F1"] = 0.0 };
--    cqa_env["[arm64] uops max"] = { ["B"] = 0.0, ["I0"] = 0.0, ["I1"] = 0.0, ["M"] = 0.0, ["L"] = 0.0, ["S"] = 0.0, ["F0"] = 0.0, ["F1"] = 0.0 };

--    -- Variables used to process DDG source nodes
--    local ddg_loops      = {};
--    local ddg_src_edges  = {};
--    -- Table that stores information to process uops that can go to multiple units
--    local uops_multi_units = {};

--    for _,insn in pairs(insns) do
--       local insn_name = insn:get_name();
--       local match = false;
--       print(insn);

--       -- Check that the loop this instruction belongs to has been processed (get list of instructions that are sources in a data dependency)
--       local loop = insn:get_loop();
--       if (loop ~= nil and not ddg_loops[loop:get_id()]) then
--          table.insert(ddg_loops, loop:get_id(), loop);
--          local ddg = loop:get_DDG();
--          for edge in pairs (ddg:get_edge2cc()) do
--             table.insert(ddg_src_edges, edge);
--          end 
--       end

--       local i = insn_variants[insn_name][FIRST_INSN_VARIANT];
--       while (i <= insn_variants[insn_name][LAST_INSN_VARIANT]) and (match == false)  do
--          match = true;

--          --- Check constraints
--          -- Check if this is the scalar/vector version of the instruction
--          if ((info[i][INSTRUCTION_VECTOR] == "SIMD") and (not insn:is_SIMD()))
--             or ((info[i][INSTRUCTION_VECTOR] == "SCALAR") and (insn:is_SIMD())) then
--             match = false;
--          end

--          -- Check constraint on operands (one operand matching the condition is sufficient)
--          if (match) and (info[i][OPERAND] ~= "") and (info[i][OPERAND] ~= nil) then
--             -- Extract the list of possible values
--             local operand_constraints = {};
--             for constraint in string.gmatch(info[i][OPERAND],"([^;]+)") do
--                table.insert(operand_constraints, constraint);
--             end

--             -- ! Test order is important ! 
--             for _,constraint in pairs(operand_constraints or {}) do
               
--                -- There should be an immediate
--                if (constraint == "^Imm$") then
--                   local immediate_found = false;
--                   for _,v in pairs (target_insn:get_operands() or {}) do
--                         if (v["type"] == MDSAPI.IMMEDIATE) then
--                            immediate_found = true;
--                         end
--                      end

--                      if (not immediate_found) then
--                         match = false;
--                         break;
--                      end

--                -- Register should not be a specific register
--                elseif (string.find(constraint, "^!%u%u$")) then
--                   local excluded_register = string.match(constraint,"^!(%u%u)$");

--                   for _,reg_name in pairs(insn:get_registers_name() or {}) do
--                      if (string.find(reg_name, excluded_register)) then
--                         match = false;
--                         break;
--                      end
--                   end

--                -- Register should be a specific register
--                elseif (string.find(constraint, "^%u%u$")) then
--                   local seeked_register = string.match(constraint,"^(%u%u)$");

--                   for _,reg_name in pairs(insn:get_registers_name() or {}) do
--                      if (not string.find(reg_name, seeked_register)) then
--                         match = false;
--                         break;
--                      end
--                   end

--                -- Register should be of the ByteWord set (B)/HalfWord set (H)/
--                -- SingleWord set (S)/DoubleWord set (D)/QuadWord set (Q)/
--                -- W set (W)/X set (X)/Vector set (V)
--                elseif (string.find(constraint, "^[%u/]+$")) then
--                   -- Extract allowed register sets
--                   local allowed_sets = {}
--                   for set in string.gmatch(constraint,"[^/]+") do
--                      table.insert(allowed_sets, set);
--                   end
                  
--                   --print("Register should be one of the following size :");
--                   --print(table.concat(allowed_sets, ","));

--                   local register_found = false;
--                   for _,set in pairs(allowed_sets or {}) do
--                      for _,reg_name in pairs(insn:get_registers_name() or {}) do
--                         if (string.find(reg_name, "^"..set)) then
--                            register_found = true;
--                         end
--                      end
--                   end

--                   if (not register_found) then
--                      match = false;
--                      break;
--                   end

--                -- There should be a specific number of registers
--                elseif (string.find(constraint, "^%d$")) then
--                   -- Count the number of registers
--                   local nb_registers = 0;
--                   for _,v in pairs (target_insn:get_operands() or {}) do
--                         if (v["type"] == MDSAPI.REGISTER) then
--                            nb_registers = nb_registers + 1 ;
--                         end
--                      end

--                      if (nb_registers ~= tonumber(constraint)) then
--                         match = false;
--                         break;
--                      end
--                end
--             end -- End of operands' constraints
--          end -- End of check on operands

--          -- Check <next property>
--          -- if (match) and ...

--          i = i + 1;
--       end

--       if (match) then
--          --print("- Match:");
--          --print("- name: "..info[i-1][INSTRUCTION_NAME]);
--          --print("- operand:"..info[i-1][OPERAND]);
--          --print("- modifier:"..info[i-1][OPERAND_MODIFIER]);
--          --print("- vector:"..info[i-1][INSTRUCTION_VECTOR]);
--          --print("- extension:"..info[i-1][INSTRUCTION_EXTENSION]);

--          -- Parse info for the matched instruction
--          local uops_to_parse_later;
--          local parsed, uops_to_parse_later = update_uops_count(cqa_env["[arm64] uops min"], cqa_env["[arm64] uops max"], info[i-1][UOPS], info[i-1][THROUGHPUT], false);
--          if (parsed == false) then
--             table.insert(uops_multi_units, {uops_to_parse_later, info[i-1][THROUGHPUT]});
--          end

--          -- If this instruction is a dependency source
--          for _,edge in pairs (ddg_src_edges) do
--             local src_node = edge:get_src_node();
--             local src_insn = src_node:get_insn();
--             if (insn:get_address() == src_insn:get_address()) then
--                --print("src dep match");
      
--                local latency_min, latency_max = info[i-1][LATENCY], info[i-1][LATENCY];
--                local data_dep = edge:get_data_dependence();
--                --print(Table:new(data_dep));

--                -- Sets a 100 cycles cost
--                if (string.find(info[i-1][LATENCY], "INF")) then
--                   latency_min, latency_max = 100, 100;
--                end
               
--                if (string.find(info[i-1][LATENCY], "-")) then
--                   latency_min,_ = string.gsub(info[i-1][LATENCY], "([%p%d]+)-([%p%d]+)", "%1");
--                   latency_max,_ = string.gsub(info[i-1][LATENCY], "([%p%d]+)-([%p%d]+)", "%2");
--                end

--                --edge:set_data_dependency_min_latency(latency_min);
--                --edge:set_data_dependency_max_latency(latency_max);

--                --data_dep["latency min"] = tonumber(latency_min, 10);   
--                --data_dep["latency max"] = tonumber(latency_max, 10);

--             end
--          end
   
--       else
--          --print("No match: "..insn_name);
--       end

--    end

--    for _,uops_info in pairs(uops_multi_units) do
--       update_uops_count(cqa_env["[arm64] uops min"], cqa_env["[arm64] uops max"], uops_info[1], uops_info[2], true);
--    end

--    for l_id, loop in pairs(ddg_loops) do
--       local recmii_min, recmii_max = loop:get_DDG():DDG_get_RecMII();
--       print("RECMII for loop "..l_id..": "..recmii_min.."-"..recmii_max);
--    end

-- end



