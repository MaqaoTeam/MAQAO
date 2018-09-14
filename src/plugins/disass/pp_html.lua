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

local function encode(str)
   return str:gsub("&","&amp;"):gsub(">", "&gt;"):gsub("<", "&lt;")
end

local function print_header(config,asmfile)
   local highlightstr = ""
   local onload = ""

   if config.dynamic_highlight then
      onload = onload .. "init_highlight_mem();"
   end

   config.printf(
[[<!DOCTYPE html>
<html>
   <head>
      <title>MAQAO - Disassembly report</title>
      <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
      <style>
         body {
            font-family:monospace;
            background-color: white;
         }
         .fct_header {
            background-color: #DDDDDD;
            margin-top:1em;
            font-family:cursive;
         }
         .fct_offset {
            color:#202020;
            float:left;
            padding-right: 1em;
         }
         .fct_label {
            float:left;
         }
         .fct_break {
            clear:both;
         }
         .fct_body {
            font-family:monospace;
            padding-left:2em;
         }

         .insn {
            line-height:1em;
            padding-left:2em;
         }
         .insn_highlight {
            background-color: #FF4444;
         }
         .insn_line{
            color:#202020;
            border: 1px dotted gray;
            text-align:right;
            clear:both;
            padding-right:0.5em;
         }
         .insn_line_begin {
            margin-top:0.8em;
            margin-bottom:0.4em;
            border-bottom: none;
         }
         .insn_line_end {
            border-top: none;
            margin-bottom:0.4em;
         }
         .insn_block{
            color: #303030;
            font-weight:bold;
            float:left;
         }
         .insn_inlined {
            margin-top: 0.2em;
            padding: 0.5em;
            padding-left: 0px;
            background-image: url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAGXRFWHRTb2Z0d2FyZQBBZG9iZSBJbWFnZVJlYWR5ccllPAAAABZJREFUeNpi2r9//38gYGAEESAAEGAAasgJOgzOKCoAAAAASUVORK5CYII=);
         }
         .insn_inlined_label {
            text-align: right;
            color: #303030;
            font-family:cursive;
            font-weight:bold;
            font-size: 0.7em;
            float:right;
            margin-top: -0.4em;
            margin-right: 0.1em;
            margin-bottom: 0.1em;
            margin-bottom: 0.2em;
         }
         .insn_inlined_label a {
            color: #303030;
         }
         .insn_offset{
            color:#202020;
            width: 10em;
            float:left;
         }
         .insn_raw {
            color: red;
            width: 22em;
            float:left;
         }
         .insn_asm {
            color:#303030;
            float:left;
            width: 24em;
         }
         .insn_asm a {
            color: #303030;
         }
         .insn_class {
            float:left;
            font-size: 70%%;
            text-align:center;
            width: 5em;
            margin-right: 0.5em;
            vertical-align:middle;
            margin-top:auto;
            margin-bottom:auto;
         }
         .insn_class div {
            border: dotted 1px black;
         }
         .insn_break {
            clear:left;
         }
         .insn_oprn_type0 { /* Register */
         }
         .insn_oprn_type1 { /* Indirect */
            color: #800000;
         }
         .insn_oprn_type2 { /* Immediate */
            font-weight:900;
         }
         .insn_oprn_type3 { /* Relative */
            font-style: italic;
            font-weight: bold;
         }
         %s
         .comment {
            color: #00688B;
            font-family: serif;
            font-size: 80%%;
         }
         div.comment {
            float:left;
            margin-left: 1em;
         }
         .loop {
            border-left: solid 1px black;
            margin-top: 0.6em;
            margin-left: 0.5em;
            padding-left: 0.4em;
            background-color: rgba(100,0,0,0.1);
         }
         .loop_label {
            font-weight:bold;
            text-decoration: underline;
         }
         #options {
            position: fixed;
            bottom: 10px;
            right: 10px;
            width: 8em;
            color: black;
            background-color: rgba(160,160,160,0.95);
            display:none;
            border: solid 1px black;
         }
         #options .title {
            font-weight: 900;
            text-align:center;
         }
         #options .subtitle {
            font-weight: bold;
            margin-left: 0.2em;
            margin-top: 0.4em;
         }
         .plusMinus {
            cursor: pointer;
            vertical-align:middle;
            line-height:1em;
         }
         .plusMinus:hover {
         }
         .plusMinus .symbol {
            float:left;
            border: 1px solid black;
            width: 1em;
            height:1em;
            margin-right: 1em;
            text-align: center;
            font-weight:bold;
         }
      </style>
      <script type="text/javascript">

         var highlighted = {}
         var colors = {
            "insn_class_simd" : "#C0C0C0",
            "insn_class_mem" : "#FFEE00",
            "insn_class_arith" : "#C0F400",
            "insn_class_control" : "#FFC500",
            "insn_class_conv" : "#A68000",
            "insn_class_math" : "#BFB630",
            "insn_class_app_specific" : "#A69B00",
            "insn_class_logic" : "#38B2CE",
            "insn_class_crypto" : "#D936C0",
            "insn_class_other" : "#60B9CE",
            "insn_class_move" : "#E972D7",
         }

         var gradientPrefix = '' 
                             

         function highlight(hlclass, enable) {

            highlighted[hlclass] = enable;

            var xs = document.getElementsByClassName(hlclass);

            for (var j = 0; j < xs.length; j++) {
               var cs = xs[j].className.split(' ');
               var color = ""
               var sep = ""
               for (var c = 0; c < cs.length; c++) {
                  if (highlighted[ cs[c] ]) {
                     color = color + sep + colors[ cs[c] ];
                     sep = ", "
                  }
               }

               xs[j].getElementsByClassName('highlighted')[0].style.background = "";

               if (color != "") {
                  var cs = color.split(", ")
                  if (cs.length == 1) {
                     xs[j].getElementsByClassName('highlighted')[0].style.backgroundColor = cs[0];
                  } else {
                     xs[j].getElementsByClassName('highlighted')[0].style.backgroundColor = "";
                     xs[j].getElementsByClassName('highlighted')[0].style.backgroundImage = gradientPrefix + "linear-gradient(to right," + color + ")";
                  }
               }
            }
         }

         function getCssValuePrefix(name, value) {
             var prefixes = ['', '-o-', '-ms-', '-moz-', '-webkit-'];

             // Create a temporary DOM object for testing
             var dom = document.createElement('div');

             for (var i = 0; i < prefixes.length; i++) {
                 // Attempt to set the style
                 dom.style[name] = prefixes[i] + value;

                 // Detect if the style was successfully set
                 if (dom.style[name]) {
                     return prefixes[i];
                 }
                 dom.style[name] = '';   // Reset the style
             }
         }

         function init_highlight_mem() {

            gradientPrefix = getCssValuePrefix('backgroundImage',
                                                'linear-gradient(#fff, #fff)');
            document.getElementById("options").style.display = "block";
         }

         function show_hide_class(className,enable) {
            var xs = document.getElementsByClassName(className)
            for (var i = 0; i<xs.length; i++) {
               xs[i].style.display = enable ? 'block' : 'none';
            }
         }

         var savedStyles = {}

         function show_hide_style(className,enable) {
            var xs = document.getElementsByClassName(className);
            if (enable) {
               var sty = savedStyles[className];
               for (var i = 0; i<xs.length; i++) {
                  xs[i].style.backgroundColor = sty["color"];
                  xs[i].style.backgroundImage = sty["image"];
                  xs[i].style.marginTop = sty["margin-top"];
                  xs[i].style.paddingTop = sty["padding-top"];
                  xs[i].style.marginBottom = sty["margin-bottom"];
                  xs[i].style.paddingBottom = sty["padding-bottom"];
               }
            } else {
               if (typeof savedStyles[className] == "undefined") savedStyles[className] = {}
               savedStyles[className]["color"] = xs[0].style.backgroundColor;
               savedStyles[className]["image"] = xs[0].style.backgroundImage;
               savedStyles[className]["margin-top"] = xs[0].style.marginTop;
               savedStyles[className]["padding-top"] = xs[0].style.paddingTop;
               savedStyles[className]["margin-bottom"] = xs[0].style.marginBottom;
               savedStyles[className]["padding-bottom"] = xs[0].style.paddingBottom;
               for (var i = 0; i<xs.length; i++) {
                  xs[i].style.backgroundColor = "rgba(0,0,0,0)";
                  xs[i].style.backgroundImage = "url()";
                  xs[i].style.marginTop = "0px";
                  xs[i].style.paddingTop = "0px";
                  xs[i].style.marginBottom = "0px";
                  xs[i].style.paddingBottom = "0px";
               }
            }
         }

         function show_hide_loop_nest(enable) {
            var className = 'loop';
            var xs = document.getElementsByClassName(className);
            if (enable) {
               var sty = savedStyles[className];
               for (var i = 0; i<xs.length; i++) {
                  xs[i].style.borderLeft = sty["border-left"];
                  xs[i].style.marginLeft = sty["margin-left"];
                  xs[i].style.paddingLeft = sty["padding-left"];
               }
            } else {
               if (typeof savedStyles[className] == "undefined") savedStyles[className] = {}
               savedStyles[className]["border-left"] = xs[0].style.borderLeft;
               savedStyles[className]["margin-left"] = xs[0].style.marginLeft;
               savedStyles[className]["padding-left"] = xs[0].style.paddingLeft;
               for (var i = 0; i<xs.length; i++) {
                  xs[i].style.borderLeft = "none";
                  xs[i].style.marginLeft = "0px";
                  xs[i].style.paddingLeft = "0px";
               }
            }
         }

         function show_hide(elem,sym) {
            elem.style.display = (elem.style.display == 'none' ? 'block' : 'none');
            sym.innerHTML = (elem.style.display == 'none' ? '+' : '&#8722;');
         }

         function options_show(enable) {
            var d = document.getElementById("options_body")
            d.style.display = (enable ? 'block' : 'none');
            d = document.getElementById("options")
            d.style.width = (enable ? '30em' : '8em');
         }
      </script>
   </head>
   <body onload="%s">
      <div id="options">
         <div class="title">
            <input type="checkbox" onClick="options_show(this.checked);" style="float:right"/>
            Options
         </div>
         <div id="options_body" style="display:none">
            <div class="subtitle">Instruction property highlighting</div>
            <div class="insn_class_mem">
               <input type="checkbox" onClick="highlight('insn_class_mem',this.checked);"/>
               <label class="highlighted">Memory access</label>
            </div>
            <div class="insn_class_simd">
               <input type="checkbox" onClick="highlight('insn_class_simd',this.checked);"/>
               <label class="highlighted">SIMD operation</label>
            </div>
            <div class="subtitle">Instruction class highlighting</div>
            <div class="insn_class_arith">
               <input type="checkbox" onClick="highlight('insn_class_arith',this.checked);"/>
               <label class="highlighted">Arithmetic instructions</label>
            </div>
            <div class="insn_class_math">
               <input type="checkbox" onClick="highlight('insn_class_math',this.checked);"/>
               <label class="highlighted">Math instructions</label>
            </div>
            <div class="insn_class_move">
               <input type="checkbox" onClick="highlight('insn_class_move',this.checked);"/>
               <label class="highlighted">Move instructions</label>
            </div>
            <div class="insn_class_conv">
               <input type="checkbox" onClick="highlight('insn_class_conv',this.checked);"/>
               <label class="highlighted">Conversion instructions</label>
            </div>
            <div class="insn_class_control">
               <input type="checkbox" onClick="highlight('insn_class_control',this.checked);"/>
               <label class="highlighted">Control instructions</label>
            </div>
            <div class="insn_class_logic">
               <input type="checkbox" onClick="highlight('insn_class_logic',this.checked);"/>
               <label class="highlighted">Logic instructions</label>
            </div>
            <div class="insn_class_other">
               <input type="checkbox" onClick="highlight('insn_class_other',this.checked);"/>
               <label class="highlighted">Other instructions</label>
            </div>
            <div class="insn_class_crypto">
               <input type="checkbox" onClick="highlight('insn_class_crypto',this.checked);"/>
               <label class="highlighted">Cryptographic instructions</label>
            </div>
            <div class="insn_class_app_specific">
               <input type="checkbox" onClick="highlight('insn_class_app_specific',this.checked);"/>
               <label class="highlighted">Application specific instructions</label>
            </div>
            <div class="subtitle">Display</div>
            <div>
               <input type="checkbox" onClick="show_hide_class('insn_block',this.checked);" checked="true"/>
               <label>Block labels</label>
            </div>
            <div>
               <input type="checkbox" onClick="show_hide_class('insn_offset',this.checked);" checked="true"/>
               <label>Instruction offset</label>
            </div>
            <div>
               <input type="checkbox" onClick="show_hide_class('insn_raw',this.checked);" checked="true"/>
               <label>Hexadecimal raw code</label>
            </div>
            <div>
               <input type="checkbox" onClick="show_hide_class('insn_class',this.checked);" checked="true"/>
               <label>Instruction class</label>
            </div>
            <div>
               <input type="checkbox" onClick="show_hide_class('insn_asm',this.checked);" checked="true"/>
               <label>Assembly code</label>
            </div>
            <div>
               <input type="checkbox" onClick="show_hide_class('comment',this.checked);" checked="true"/>
               <label>Comments</label>
            </div>
            <div>
               <input type="checkbox" onClick="show_hide_class('insn_line',this.checked);" checked="true"/>
               <label>Source file/line</label>
            </div>
            <div>
               <input type="checkbox" onClick="show_hide_class('fct_header',this.checked);" checked="true"/>
               <label>Function header</label>
            </div>
            <div>
               <label>Loop:</label>
               <input type="checkbox" onClick="show_hide_class('loop_label',this.checked);" checked="true"/>
               <label>label</label>
               <input type="checkbox" onClick="show_hide_style('loop',this.checked);" checked="true"/>
               <label>region</label>
               <input type="checkbox" onClick="show_hide_loop_nest(this.checked);" checked="true"/>
               <label>nesting</label>
            </div>
            <div>
               <input type="checkbox" onClick="show_hide_style('insn_inlined',this.checked);show_hide_class('insn_inlined_label',this.checked);" checked="true"/>
               <label>Inlined region</label>
            </div>
         </div>
      </div>
]], highlightstr, onload)
end

local function print_footer(config,asmfile)
   config.printf(
[[
   </body>
</html>
]])
end

local function print_fct_header(config,fct,name)
   local addr = fct:get_first_insn():get_address()

   config.printf([[
   <div class="fct_header">
      <div class="fct_offset">%016x</div>
      <div class="plusMinus fct_label" onclick="show_hide(document.getElementById('fct%d'),document.getElementById('fct%d_sym'))"><div id="fct%d_sym" class="symbol">&#8722;</div>%s</div>
      <div class="fct_break"></div>
   </div>]], addr, fct:get_id(), fct:get_id(), fct:get_id(), encode(name))
end

local function print_fct_body_header(config,fct)
   config.printf([[<div class="fct_body" id="fct%d">]], fct:get_id())
end

local function print_fct_footer(config,fct)
   config.printf("</div>")
end

local function print_line_begin(config,file,line)
   config.printf("<div class=\"insn_line insn_line_begin\">%s:%d</div>", file,line)
end

local function print_line_end(config,file,line)
   config.printf([[<div class="insn_line insn_line_end">&nbsp;</div>]])
end

local function print_block_begin(config,block,label,isBeginLoop,comment)
  
   if not isBeginLoop and block:is_loop_entry() then
      local loop = block:get_loop()
      comment = comment .. string.format("Loop %d (entry)", loop:get_id())
   end

   if config.show_comment and comment ~= "" then 
      comment = "; "..comment
   end

   config.printf([[<div class="insn_block">%d:</div><div class="comment">%s</div><div style="clear:both"></div>]], block:get_id(), comment)
end

local function print_block_end(config,block)
end

local function print_inline_begin(config,fct)
   local addr = fct:get_first_insn():get_address()
   config.printf([[<div class="insn_inlined"><div class="insn_inlined_label">Inlined from function "<a href="#offset_%x">%s</a>"</div>]], addr, encode(fct_expected_name(config,fct)))
end

local function print_inline_end(config,fct)
   config.printf([[<div style="clear:both"></div></div>]])
end

local function print_insn(config,fct,i,comment,class)

   local offstr = ""
   local rawstr = ""
   local asmstr = ""
   local asmclassstr = ""
   local cmtstr = ""
   local classstr = ""

   if config.show_offset then
      local addr = i:get_address()
      local fmt = [[<div class="insn_offset">%x:</div>]]
      offstr = string.format(fmt, addr)
   end

   if config.show_raw then
      local raw = trim(i:get_coding())
      local fmt = [[<div class="insn_raw">%-21s</div>]]
      rawstr = string.format(fmt,raw)
   end

   if config.show_asm then
      asmstr = i:get_name():lower()

      local sep = " "
      for iop,op in pairs(i:get_operands()) do
         local optyp = i:get_oprnd_type(iop-1)
         local opstr = encode(i:get_oprnd_str(iop-1):lower())
         
         -- Remove "<my_function_name>" from addresses as we show them in comments
         if optyp == 3 then
            opstr = string.gmatch(opstr, '%S+')()
         end
         asmstr = string.format([[%s%s<span class="insn_oprn_type%d">%s</span>]], asmstr, sep, optyp, opstr)
         sep = ", "

         if optyp == 1 then
            asmclassstr = string.format("%sinsn_class_mem ", asmclassstr)
         end
      end

      if i:is_SIMD() then
         asmclassstr = string.format("%sinsn_class_simd ", asmclassstr)
      end
      asmclassstr = string.format("%sinsn_class_%s ", asmclassstr, class)


      if i:is_branch() or i:is_call() then
         target = i:get_branch_target()
         if target ~= nil then 
            local href = string.format("offset_%x",target:get_address())
            local js = string.format([[
               onmouseover="document.getElementById('insn_%x').className='insn insn_highlight'"
               onmouseout="document.getElementById('insn_%x').className='insn'"]], 
               target:get_address(), target:get_address())
            asmstr = string.format([[<a href="#%s" %s>%s</a>]], href, js, asmstr)
         end
      end
      asmstr = string.format([[<div class="insn_asm highlighted">%s</div>]], asmstr)
   end

   if config.show_comment and comment ~= "" then
      cmtstr = string.format([[<div class="comment">; %s</div>]],comment)
   end

   if config.show_class then
      classstr = string.format([[<div class="insn_class"><div>%s</div></div>]], class)
   end

   local breakstr = [[<div class="insn_break"></div>]]

   config.printf([[<div class="insn" id="insn_%x"><a name="offset_%x"></a><div class="%s">%s%s%s%s%s%s</div></div>]],i:get_address(),i:get_address(),asmclassstr,offstr,rawstr,classstr,asmstr,cmtstr,breakstr)
end

local function print_loop_begin(config,loop,insn)
   local id = ""
   local label = ""

   if insn:get_block():is_loop_entry() then
      id = string.format("loop%d", loop:get_id())
      label = string.format("Loop %d (entry)", loop:get_id())
   else
      id = string.format("loop%d_cont%x", loop:get_id(), insn:get_address())
      label = string.format("Loop %d (continued)", loop:get_id())
   end

   config.printf([[
      <div class="loop">
      <div class="loop_label"><div onclick="show_hide(document.getElementById('%s'),document.getElementById('%s_sym'))" class="plusMinus"><div id="%s_sym" class="symbol">&#8722;</div>%s</div><div style="clear:both"></div></div>
      <div id="%s">]], id, id, id, label, id)
end

local function print_loop_end(config,loop,insn)
   config.printf([[<div class="loop_label">Loop %d exit</div></div></div>]], loop:get_id())
end

pp_html = {
   print_header = print_header,
   print_footer = print_footer,
   print_fct_header = print_fct_header,
   print_fct_body_header = print_fct_body_header,
   print_fct_footer = print_fct_footer,
   print_line_begin = print_line_begin,
   print_line_end = print_line_end,
   print_block_begin = print_block_begin,
   print_block_end = print_block_end,
   print_insn = print_insn,
   print_inline_begin = print_inline_begin,
   print_inline_end = print_inline_end,
   print_loop_begin = print_loop_begin,
   print_loop_end = print_loop_end
}

