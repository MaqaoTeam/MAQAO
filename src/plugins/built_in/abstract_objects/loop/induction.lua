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
module("loop.induction",package.seeall) 

local function get_invariants_first_pass_insn (i, invariants, writes)
   local are_all_rd_ops_consts, nb_writes, written;
 
   --print(i);
   oprds = i:get_operands();
   are_all_rd_ops_consts = true;
   nb_writes = 0;
   for k,op in ipairs(oprds) do
      typ = op.type;
      if(op.write == true) then  
         written = i:get_oprnd_str(k-1);
         nb_writes = nb_writes + 1;     
         if(writes[written] ~= nil) then
            writes[written] = writes[written] + 1;
         else
            writes[written] = 1;
         end
         --TODO if register => write widest reg of family (i.e. XMM => YMM)
      end
      if(typ ~= Consts.OT_IMMEDIATE and op.read == true) then               
         are_all_rd_ops_consts = false;
      end            
   end 
   if(are_all_rd_ops_consts == true and written ~= nil) then
      invariants.insn:insert(i);
   end
end

local function get_invariants_second_pass_insn (i, invariants, writes)
   local are_all_rd_ops_inv, sop;
 
   oprds = i:get_operands();
   are_all_rd_ops_inv = true;
   --Find out if insn is invariant: all ops must be invariant
   for k,op in ipairs(oprds) do
      local typ = op.type;          
      sop = i:get_oprnd_str(k-1);
    
      if(typ == Consts.OT_REGISTER or typ == Consts.OT_MEMORY) then               
         if(type(writes[sop]) == "number") then
            are_all_rd_ops_inv = false;
         end
         --Process registers and memory references
         if(typ == Consts.OT_REGISTER and writes[sop] == nil) then
            invariants.oprnd[sop] = true;
            invariants.reg[sop] = true;
         elseif(typ == Consts.OT_MEMORY) then
            local bic  = 0;--check if registers involved in address computation are written
            local bicw = 0;

            for _,sop in ipairs(op.value) do
               if(sop.typex == "base" or sop.typex == "index") then
                  local r = "%"..sop.value;

                  bic = bic + 1;                        
                  if(writes[r] == nil) then
                     bicw = bicw + 1;
                     invariants.reg[r] = true;
                  end
               end                              
            end
            if(bic == bicw) then
              invariants.oprnd[sop] = true;
            end
         end
      end
   end
 --TODO : verify if implicit operand(s) instructions are handled properly
 if(are_all_rd_ops_inv == true and sop ~= nil) then
    invariants.insn:insert(i);
 end
end

function loop:get_invariants()
   local invariants = Table:new({insn = Table:new(), oprnd = Table:new(), reg = Table:new()});
   local writes     = {}; 
   --First pass => add definitions which operand is constant
   for b in self:blocks() do
      for i in b:instructions() do
         get_invariants_first_pass_insn (i, invariants, writes)
      end      
   end
   --Table:tostring(writes);
   --Table:tostring(invariants);
   --Second pass => add definitions which operand is an invariant or defined outside loop scope
   for b in self:blocks() do
      for i in b:instructions() do
         get_invariants_second_pass_insn (i, invariants, writes)
      end      
   end  
   --print('#################Invariants');
   --Table:tostring(invariants);
   
   return invariants,writes;
end

function loop:get_invariants_from_insnlist(insns)
   local invariants = Table:new({insn = Table:new(), oprnd = Table:new(), reg = Table:new()});
   local writes     = {}; 
   --First pass => add definitions which operand is constant
   for _,i in ipairs (insns) do
      get_invariants_first_pass_insn (i, invariants, writes)
   end
   --Table:tostring(writes);
   --Table:tostring(invariants);
   --Second pass => add definitions which operand is an invariant or defined outside loop scope
   for _,i in ipairs (insns) do
      get_invariants_second_pass_insn (i, invariants, writes)
   end  
   --print('#################Invariants');
   --Table:tostring(invariants);
   
   return invariants,writes;   
end

function _find_init_value(src_block,register) 
   local bound;
   local found, pred, nb_preds, entry, preds;
   local entries = src_block:get_loop():get_entries();
   
   for _,b in pairs(entries) do
     entry = b;
     break;
   end

   preds = Table:new(entry:get_predecessors());
   Table:tostring(preds);
   found = false;
   
   if(preds ~= nil) then
      for k,p in pairs(preds) do
         
         if(p:get_loop() ~= nil and p:get_loop():get_id() == entry:get_loop():get_id()) then
            preds[k] = nil;
         end
      end
      nb_preds = preds:get_size();
   else
      nb_preds = 0;
   end
   print('Before while nb_preds = ',nb_preds,
         string.format("%x",entry:get_first_insn():get_address()),
         entry:get_function():get_CFG_file_path());
   while(nb_pred == 1 and found == false) do
      local a;
      pred = preds[1];print('Processing block',pred);      
      --look for definition of "register" (as param) where op is immediate
      for i in b:instructions() do
         oprds = i:get_operands();
         for k,op in ipairs(oprds) do
            str = i:get_oprnd_str(k-1);
            if(str == register and op.type == Consts.OT_REGISTER and op.write == true) then  
               print('Occurence',i);
            end
         end
      end                  
      preds = pred:get_predecessors();
      if(preds ~= nil) then
         nb_preds = #preds;
      else
         nb_preds = 0;
      end                
   end
   
   return bound;
end

--We look for i + a or i - a expressions where i is the induction and -/+ a is a constant value 
--TODO look for all possible add/sub insns or handle it using families 
function _find_induc_val(insn) 
   local val;
   local valid_insns = Table:new({
      'ADD', 'SUB', 'INC', 'DEC'
   });

   name = insn:get_name();

   if(valid_insns:containsv(name)) then 
      if(name == "ADD" or name == "SUB") then
         oprds = insn:get_operands();
         --Const static increment
         --TODO: find more generic approach
         if(oprds[1].type == Consts.OT_IMMEDIATE) then
            val = oprds[1].value;
            valtype = "Const";
         elseif(oprds[1].type == Consts.OT_REGISTER) then       
            valtype = "Invar";
         end
      elseif(name == "DEC" or name == "INC") then
         val = 1;
         valtype = "Const";
      end
   end
   
   return val,valtype;
end

function _find_induc_derived_val(insn, inducs, invs) 

end

local function get_basic_inductions_insn (i, inductions)
   local reg_rw, induc_progression;
   oprds = i:get_operands();
 
   for k,op in ipairs(oprds) do
      typ = op.type;          
      written = i:get_oprnd_str(k-1);
    
      if(typ == Consts.OT_REGISTER or typ == Consts.OT_MEMORY) then              
         if(op.read == true and op.write == true) then
            reg_rw = true;
         end
      end
   end  
   --Simple/Constant induction = R/W operand + an immediate induction factor         
   if(reg_rw == true and written ~= nil) then
      local val, valtype = _find_induc_val(i);

      if((valtype == 'Const' and val ~= nil) or (valtype == 'Invar' and val == nil)) then
        --switch the register name to it family representative
        local reg_name = "%"..string.match(written,"%w+");
        if (Stream.register_families_rev[reg_name] ~= nil) then
          written = "%"..Stream.register_families_rev[reg_name];
          inductions[written] = {insn = i, val = val, valtype = valtype};
        end
      end
   end
end

local function get_derived_inductions_insn (i, inductions, inv)
   local reg_rw, induc_progression;
   local nb_ops, nb_ops_counted;
  
   if(i:is_store() == true) then
       local are_all_rd_ops_ok, op_regs, op_dest_typ, op_dest_idx;
      
       op_dest_idx = i:get_operand_dest_index();
       op_dest_typ = i:get_oprnd_type(op_dest_idx);
       --TODO: extend to RIP and RSP/RBP ?
       --print("In repeat",op_dest_typ,i:has_src_mem_oprnd(),i);
       if(op_dest_typ == Consts.OT_REGISTER and i:has_src_mem_oprnd() == true) then
         --print("In repeat");
         op_src_idx = i:get_operand_src_index();
         op_src_typ = i:get_oprnd_type(op_src_idx);

         if(op_src_typ == Consts.OT_REGISTER or op_src_typ == Consts.OT_MEMORY) then
           --TODO: create function that reuses code from LEA case
         end
      end
   --Specific case LEA insn is neither a load nor a store
   elseif(i:get_name() == "LEA") then
     local ops, op, reg_induc, reg_induc_val, val, base, baset, index, indext, scale, dreg;
     ops  = i:get_operands();
     op   = ops[1];--mem operand
     opd  = Stream:__get_oprnd_decomp(op.value);
     dreg = "%"..Stream.register_families_rev["%"..ops[2].value]; --reg operand
    
     if(opd.base ~= nil) then
        base = opd.base;
        if(inductions["%"..Stream.register_families_rev["%"..base]] ~= nil) then
           baset = "IND";
        elseif(inv.reg["%"..base]) then
           baset = "INV";
        else
           baset = "NA";
        end
     end
     if(opd.index ~= nil) then
        index = opd.index;
        if(inductions["%"..Stream.register_families_rev["%"..index]] ~= nil) then
           indext = "IND";
        elseif(inv.reg["%"..index]) then
           indext = "INV";
        else
           indext = "NA";
        end
     end
     if(opd.scale ~= nil) then scale = opd.scale; else scale = 1; end
     --At least one induction
     if(baset == "IND" or indext == "IND") then -- 3 cases             
        if(baset == "IND" and indext ~= "IND") then --shou
           local bas = 0;
           if(base ~= nil) then bas = "%"..Stream.register_families_rev["%"..base] end;
           if(inductions[bas].val ~= nil) then
              reg_induc_val = inductions[bas].val;
           end
        elseif(baset ~= "IND" and indext == "IND") then
           local ind = 0;                   
           if(index ~= nil) then ind =  "%"..Stream.register_families_rev["%"..index]; end
           if(inductions[ind].val ~= nil) then 
              reg_induc_val = inductions[ind].val * scale;  
           end 
        elseif(baset == "IND" and indext == "IND") then
           local bas =  "%"..Stream.register_families_rev["%"..base];
           local ind =  "%"..Stream.register_families_rev["%"..index];
           local r1  = inductions[bas].val;
           local r2  = inductions[ind].val;
           if(inductions[bas].val ~= nil and inductions[ind].val ~= nil) then
              reg_induc_val = r1 + r2 * scale;
           end
        end
     --else only produces in invariant
     end
     -- reg_induc_val can be nil if induction values are unknown
     inductions[dreg] = {insn = i, val = reg_induc_val};
   end
end

function loop:get_inductions() 
   local inductions = Table:new();   
   local exit_blocks = Table:new();
   local exits = Table:new();   
   local inv,writes = self:get_invariants();
   --[[
   print("Writes");
   Table:tostring(writes);
   print("Invariants");
   Table:tostring(inv);
   --]]  
   --Basic inductions
   --N.B.: this could be done in loop invariant computation + Iterate over paths
   for b in self:blocks() do
      --Find exit blocks
      if(b:is_loop_exit() == true) then
         exit_blocks:insert(b);
      end
      for i in b:instructions() do
         get_basic_inductions_insn (i, inductions)
      end      
   end
   --inductions:tostring();
   --Derived inductions
   --Instructions which operands are exclusively either simple inductions, invariants or immediates   
   local found_new_induction;
   -- Iterate until no new induction can be found
   repeat 
      found_new_induction = false;
      for b in self:blocks() do
	  --Find exit blocks
        if(b:is_loop_exit() == true) then
          exit_blocks:insert(b);
        end
        for i in b:instructions() do
           get_derived_inductions_insn (i, inductions, inv)
        end 
     end       
   until (found_new_induction == false);
   --inductions:tostring();
   -- Look for exit condition reg/mem
   for _,b in pairs(exit_blocks) do
      local linsn = b:get_last_insn();
      
      if(linsn:is_branch()) then
         --look for last instruction modifying the compare flags
         local iprev = linsn:get_prev();
         oprds = iprev:get_operands();
         for k,op in ipairs(oprds) do
            typ = op.type;
            if(typ == Consts.OT_REGISTER or typ == Consts.OT_MEMORY) then              
               str = iprev:get_oprnd_str(k-1);
               exits[str] = iprev;
            end
         end       
      else
         --[[
         if(linsn:is_call() or is_return()) then
            
         else
            
         end--]]
         --print("Exit block not ending with Branch");
      end
   end
   --promote induction to loop iterator 
   for reg,val in pairs(inductions) do 
      for e,v in pairs(exits) do
         if(reg == e) then
            local induc_val, bound;
            b = v:get_block();
            if(writes[i] == 1) then
               --bound = _find_init_value(b,reg);print("_find_value(",b,",",reg,")");
            else
               bound = nil; 
            end
               inductions[reg].exit_insn = v;
               inductions[reg].bound = bound;
            break;
         end
      end
   end

   --[[
   print("Invariants");
   Table:tostring(inv);
   print("Inductions");
   Table:tostring(inductions);
   print("Exits");
   Table:tostring(exits);
   print("Iterators");
   Table:tostring(iterators);--]]
   
   return inductions, inv;
end

function loop:get_inductions_from_insnlist(insns) 
   local inductions = Table:new();   
   local exit_blocks = Table:new();
   local exits = Table:new();   
   local inv,writes = self:get_invariants();
   --[[
   print("Writes");
   Table:tostring(writes);
   print("Invariants");
   Table:tostring(inv);
   --]]  
   --Basic inductions
   --N.B.: this could be done in loop invariant computation + Iterate over paths
   for _,i in ipairs(insns) do
      get_basic_inductions_insn (i, inductions)
   end      
   --inductions:tostring();
   --Derived inductions
   --Instructions which operands are exclusively either simple inductions, invariants or immediates   
   local found_new_induction;
   -- Iterate until no new induction can be found
   repeat 
      found_new_induction = false;
      for _,i in ipairs(insns) do
         get_derived_inductions_insn (i, inductions, inv)
      end 
   until (found_new_induction == false);
   --inductions:tostring();
   -- Look for exit condition reg/mem
   local linsn = insns[#insns];
   if(linsn:is_branch()) then
      --look for last instruction modifying the compare flags
      local iprev = linsn:get_prev();
      oprds = iprev:get_operands();
      for k,op in ipairs(oprds) do
         typ = op.type;
         if(typ == Consts.OT_REGISTER or typ == Consts.OT_MEMORY) then              
            str = iprev:get_oprnd_str(k-1);
            exits[str] = iprev;
         end
      end       
   else
      --[[
         if(linsn:is_call() or is_return()) then
         
         else
         
         end--]]
      --print("Exit block not ending with Branch");
   end
   --promote induction to loop iterator 
   for reg,val in pairs(inductions) do 
      for e,v in pairs(exits) do
         if(reg == e) then
            local induc_val, bound;
            b = v:get_block();
            if(writes[i] == 1) then
               --bound = _find_init_value(b,reg);print("_find_value(",b,",",reg,")");
            else
               bound = nil; 
            end
               inductions[reg].exit_insn = v;
               inductions[reg].bound = bound;
            break;
         end
      end
   end

   --[[
   print("Invariants");
   Table:tostring(inv);
   print("Inductions");
   Table:tostring(inductions);
   print("Exits");
   Table:tostring(exits);
   print("Iterators");
   Table:tostring(iterators);--]]
   
   return inductions, inv;
end
