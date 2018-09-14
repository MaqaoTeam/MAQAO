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

--- Module cqa.unroll
-- Defines functions to compute, return or print the unroll factor of a source loop
module ("cqa.api.unroll", package.seeall)

--- [Local function, used only by use_Newton_Raphson and get_metric_values]
-- If more than one path, consider only the first one
-- @param cqa_results Table listing static analysis results
-- @param bin_loop loop identifier
-- @return static analysis results for bin_loop
local function get_cqa_results (cqa_results, bin_loop)
   if (cqa_results [bin_loop].common ["nb paths"] > 1) then
      Debug:warn ("Limiting unroll analysis to the first path of a multi-paths loop");
   end

   return cqa_results [bin_loop].paths[1];
end

--- [Local function, used only by get_metric_values]
-- Checks whether a source loop uses a Newton-Raphson method for fast division or
-- square root and contains the corresponding slow instruction in peel/tail loops.
-- Occurs for single-precision loops vectorized by Intel compilers (tested: 12.x).
-- @param src_loop Table representing one source loop
-- @param cqa_results Table listing static analysis results
-- for all the loops in the function currently analysed
-- @return boolean false, "div", "sqrt" or "div+sqrt"
local function use_Newton_Raphson (src_loop, cqa_results)
   -- number of binary/assembly loops with:
   local div_no_rcp    = 0; -- at least one division and no reciprocal
   local no_div_rcp    = 0; -- no division and at least one reciprocal
   local sqrt_no_rsqrt = 0; -- at least one sqrt and no reciprocal sqrt
   local no_sqrt_rsqrt = 0; -- no sqrt and at least one reciprocal sqrt

   local bin_loops = {}
   for _, bin_loop in pairs (src_loop ["loop IDs"]) do
      if (cqa_results [bin_loop].paths ~= nil) then
         table.insert (bin_loops, bin_loop)
      end    
   end

   -- for each binary loop
   for _, bin_loop in ipairs (bin_loops) do
      local sr = get_cqa_results (cqa_results, bin_loop);

      -- number of floating-point (FP) operations
      local nb_div   = sr ["FP ops"]["div"];
      local nb_rcp   = sr ["FP ops"]["rcp"];
      local nb_sqrt  = sr ["FP ops"]["sqrt"];
      local nb_rsqrt = sr ["FP ops"]["rsqrt"];

      if ((nb_div > 0) and (nb_rcp == 0)) then
         div_no_rcp = div_no_rcp + 1;
      elseif ((nb_div == 0) and (nb_rcp > 0)) then
         no_div_rcp = no_div_rcp + 1;
      end

      if ((nb_sqrt > 0) and (nb_rsqrt == 0)) then
         sqrt_no_rsqrt = sqrt_no_rsqrt + 1;
      elseif ((nb_sqrt == 0) and (nb_rsqrt > 0)) then
         no_sqrt_rsqrt = no_sqrt_rsqrt + 1;
      end
   end

   local fast_div  = (div_no_rcp    > 0) and (no_div_rcp    > 0);
   local fast_sqrt = (sqrt_no_rsqrt > 0) and (no_sqrt_rsqrt > 0);

   if (fast_div and fast_sqrt) then return "div+sqrt" end
   if (fast_div ) then return "div"  end
   if (fast_sqrt) then return "sqrt" end
   return false; -- fast_div and fast_sqrt are false
end

--- [Local function, used only by compute_src_unroll_factor]
-- Returns metric values for a source loop from static analysis results.
-- Structured as a 2D array:
-- * 1st dimension: binary loop IDs, "min", "max", "max / min"
-- * 2nd dimension: metrics like "nb FP div operations"
-- @param src_loop Table representing one source loop
-- @param cqa_results Table listing static analysis results
-- for all the loops in the function currently analysed
-- @return metric values (2D array)
local function get_metric_values (src_loop, cqa_results)
   local metric_values = {};
   local min = {};
   local max = {};
   local metrics = {"nb FP add-sub operations",
                    "nb FP mul operations",
                    "nb FP fma operations",
                    "nb FP div operations",
                    "nb FP rcp operations",
                    "nb FP sqrt operations",
                    "nb FP rsqrt operations",
                    "bytes loaded",
                    "bytes stored"};
   local bin_loops = {}
   for _, bin_loop in pairs (src_loop ["loop IDs"]) do
      if (cqa_results [bin_loop].paths ~= nil) then
         table.insert (bin_loops, bin_loop)
      end    
   end

   -- compute the min and the max value for all metrics
   for _, bin_loop in ipairs (bin_loops) do
      local sr = get_cqa_results (cqa_results, bin_loop);

      local bsr = sr ["bytes stack ref"];
      local bsr_load, bsr_store;
      if (type (bsr) == "table") then
	 bsr_load  = bsr.load;
	 bsr_store = bsr.store;
      end

      metric_values [bin_loop] = {
         ["bytes loaded"] = (sr ["bytes loaded"] or 0) - (bsr_load or 0),
         ["bytes stored"] = (sr ["bytes stored"] or 0) - (bsr_store or 0),
      }
      for op_type,nb in pairs (sr ["FP ops"]) do
         metric_values [bin_loop]["nb FP "..op_type.." operations"] = nb
      end

      -- for each metric, update min and max
      for metric, metric_value in pairs (metric_values [bin_loop]) do
         if (min [metric] ~= nil) then
            min [metric] = math.min (min [metric], metric_value);
            max [metric] = math.max (max [metric], metric_value);
         else
            min [metric] = metric_value;
            max [metric] = metric_value;
         end
      end
   end

   metric_values ["min"] = {};
   metric_values ["max"] = {};
   metric_values ["max / min"] = {};

   -- for each metric, store min and max and compute max / min
   for _, metric in pairs (metrics) do
      metric_values ["min"][metric] = min [metric];
      metric_values ["max"][metric] = max [metric];

      if (min [metric] ~= nil and
          min [metric] ~= 0   and
          max [metric] ~= nil) then
         metric_values ["max / min"][metric] = max [metric] / min [metric];
      end
   end

   local newton_raphson = use_Newton_Raphson (src_loop, cqa_results);

   if (newton_raphson ~= false) then
      if (string.find (newton_raphson, "div") ~= nil) then
         metric_values ["max / min"]["nb FP div operations"] = nil;
         metric_values ["max / min"]["nb FP rcp operations"] = max ["nb FP rcp operations"] / max ["nb FP div operations"];
      end

      if (string.find (newton_raphson, "sqrt") ~= nil) then
         metric_values ["max / min"]["nb FP sqrt operations" ] = nil;
         metric_values ["max / min"]["nb FP rsqrt operations"] = max ["nb FP rsqrt operations"] / max ["nb FP sqrt operations"];
      end
   end

   return metric_values;
end

--- Computes the unroll factor for a set of source loops from static analysis results.
-- The unroll factor is written to the "unroll factor" field of each source loop.
-- @param src_loop source loop (set of binary loops sharing same last source line)
-- @param cqa_results Table listing static analysis results
-- for all the loops in the function currently analysed
function compute_src_unroll_factor (src_loop, cqa_results)
   local function is_integer (x)
      local _, frac_part = math.modf (x);
      return (frac_part == 0);
   end

   local function found_at_least_one_integer_ratio (metric_values)
      for _,v in pairs (metric_values ["max / min"]) do
         if (is_integer (v)) then return true end
      end

      return false;
   end

   local function get_unanimity (metric_values)
      local first_val;

      for _, v in pairs (metric_values ["max / min"]) do
         if (v > 0) then
            if (first_val == nil) then
               first_val = v;
            elseif (v ~= first_val) then
               return nil;
            end
         end
      end

      return first_val;
   end

   local function get_majority (metric_values)
      local popcnt = {};
      local tot_nb = 0;
      local max_nb = 0;
      local max_val;

      for _,v in pairs (metric_values ["max / min"]) do
         if (v > 0 and is_integer (v)) then
            tot_nb = tot_nb + 1;
            popcnt [v] = (popcnt [v] or 0) + 1;
            max_nb = math.max (max_nb, popcnt [v]);
            max_val = v;
         end
      end

      return max_nb, tot_nb, max_val;
   end

   local function set_src_loop_unroll_info (src_loop, unroll_factor, confidence_level)
      src_loop ["unroll factor"] = unroll_factor;

      src_loop ["unroll metric"] = {};
      for k,v in pairs (src_loop ["unroll metric values"]["max / min"]) do
         if (v == unroll_factor) then
            table.insert (src_loop ["unroll metric"], k);
         end
      end

      src_loop ["unroll confidence level"] = confidence_level;
   end

   local metric_values = get_metric_values (src_loop, cqa_results);
   src_loop ["unroll metric values"] = metric_values;

   if (found_at_least_one_integer_ratio (metric_values)) then
      local unanimity = get_unanimity (metric_values);
      if (unanimity ~= nil) then
         set_src_loop_unroll_info (src_loop, unanimity, "max");

      else
         local store_ratio = metric_values ["max / min"] ["bytes stored"];
         if (store_ratio ~= nil and store_ratio ~= 0 and is_integer (store_ratio)) then
            set_src_loop_unroll_info (src_loop, store_ratio, "high");

         else
            local max_nb, tot_nb, max_val = get_majority (metric_values);
            set_src_loop_unroll_info (src_loop, max_val, "medium");

            if ((max_nb / tot_nb) <= 0.5) then
               src_loop ["unroll confidence level"] = "low";
            end
         end -- integer stored bytes ratio
      end -- unanimity
   end -- at least one integer ratio
end

--- [Local function, used only by add_unroll_factor_in_src_loop]
-- Returns the unroll factor of a binary loop, compared to the "smallest"
-- binary loop in the same source loop.
-- @param src_loop Table representing the source loop
-- @param bin_loop MAQAO ID of a binary loop
-- @return unroll factor (number or nil)
local function get_bin_unroll_factor (src_loop, bin_loop)
   if (src_loop ["unroll metric values"] [bin_loop] == nil) then return nil end

   local at_least_one_failed_min = false;
   local at_least_one_failed_max = false;
   local candidate_unroll_factor = Table:new();

   for _,v in pairs (src_loop ["unroll metric"]) do
      local bin = src_loop ["unroll metric values"] [bin_loop][v];
      local min = src_loop ["unroll metric values"] ["min"   ][v];
      local max = src_loop ["unroll metric values"] ["max"   ][v];

      if (bin ~= min) then at_least_one_failed_min = true end
      if (bin ~= max) then at_least_one_failed_max = true end

      if (min > 0 and bin > min and bin < max) then
         candidate_unroll_factor [v] = bin / min;
      end
   end

   if (at_least_one_failed_min and not at_least_one_failed_max) then return src_loop ["unroll factor"] end
   if (at_least_one_failed_max and not at_least_one_failed_min) then return 1 end

   -- Detect middle-size main loops (bigger than the smallest loop but smaller than the biggest loop)
   -- TODO: use something better than arithmetic mean (reuse compute_src_unroll_factor)
   local sum = 0;
   local max = 1;
   for _,v in pairs (candidate_unroll_factor) do
      sum = sum + v;
      max = math.max (max, v);
   end

   if (max < 2) then return 1 end
   if (sum > 0) then return sum / candidate_unroll_factor:get_size() end;
end

-- K1OM specific
local function get_stride (cqa_result)
   local sr = cqa_result;
   if (cqa_result.paths [1] ~= nil) then sr = cqa_result.paths [1] end

   local packed_ratio = sr ["packed ratio"]["all"];
   if ((packed_ratio ==  nil) or
       (packed_ratio == "NA") or
       (packed_ratio  <   80)) then
      return 1;
   end

   local pt = sr ["pattern"]
   if (pt ["PD"  ] > 0) then return  8 end
   if (pt ["PS"  ] > 0) then return 16 end
   if (pt ["INTQ"] > 0) then return  8 end
   if (pt ["INTD"] > 0) then return 16 end

   return 1;
end
-- END OF K1OM SPECIFIC

function add_unroll_factor_in_src_loop (src_loop, cqa_results, arch)
   local max_stride = 1; -- k1om: unroll factor multiplicator
   local src_unroll_info;

   -- K1OM SPECIFIC
   if (arch == "k1om") then
      local src_unroll_factor = src_loop ["unroll factor"];

      for _,bin_loop in pairs (src_loop ["loop IDs"]) do
         local stride = get_stride (cqa_results [bin_loop]);

         if (stride > 1) then
            if (type (src_unroll_factor) == "number" and src_unroll_factor > 1) then
               local bin_unroll_factor = get_bin_unroll_factor (src_loop, bin_loop);

               if (type (bin_unroll_factor) == "number") then
                  cqa_results [bin_loop].common ["unroll factor"] = get_bin_unroll_factor (src_loop, bin_loop) * stride;
               end
            else
               cqa_results [bin_loop].common ["unroll factor"] = stride;
            end

            max_stride = math.max (max_stride, stride);
         end
      end
   end
   -- END OF K1OM SPECIFIC

   if (#src_loop ["loop IDs"] == 1) then
      src_unroll_info = "not unrolled or unrolled with no peel/tail loop";

   else
      local src_unroll_factor = src_loop ["unroll factor"];

      if (type (src_unroll_factor) == "number" and src_unroll_factor > 1) then
         -- for each binary loop
         for _,bin_loop in pairs (src_loop ["loop IDs"]) do
            local bin_unroll_factor = get_bin_unroll_factor (src_loop, bin_loop);

            -- Generic unrolled loops
            if (type (bin_unroll_factor) == "number") then
               cqa_results [bin_loop].common ["is main/unrolled"] = (bin_unroll_factor == src_unroll_factor);

               if (arch ~= "k1om") then
                  cqa_results [bin_loop].common ["unroll factor"] = bin_unroll_factor;
               end
            end
         end -- for each binary loop

         src_unroll_info = "unrolled by " .. src_unroll_factor;

      else -- if multi-versionned loop
         src_unroll_info = "multi-versionned";
      end
   end -- if more than one binary loop

   -- K1OM SPECIFIC
   if (max_stride > 1) then
      for _,bin_loop in pairs (src_loop ["loop IDs"]) do
         if (cqa_results [bin_loop].common ["first insn"]:get_name() == "VPCMPGTD") then
            local src_unroll_factor = src_loop ["unroll factor"];

            if (type (src_unroll_factor) ~= "number") then src_unroll_factor = 1 end

            src_unroll_factor = src_unroll_factor * max_stride;
            src_unroll_info = "unrolled by " .. src_unroll_factor;
            src_loop ["unroll factor"] = src_unroll_factor;
            break;
         end
      end
   end
   -- END OF K1OM SPECIFIC

   -- Tag binary loops as:
   --  * main (unrolled and/or vectorized),
   --  * intermediate (unrolled/vectorized tail),
   --  * peel/tail (not unrolled and not vectorized)
   if (string.find (src_unroll_info, "^unrolled") ~= nil) then
      src_loop.main         = {};
      src_loop.intermediate = {};
      src_loop.peel_or_tail = {};

   end

   src_loop ["unroll info"] = src_unroll_info;
end
