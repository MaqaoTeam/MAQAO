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

---	@class SVG Class

SVG = {};
local svg_meta = {};

---	Creates a list data structure
--	@return New instance
function SVG:new(startp,size)
	local svg = {};
	svg.printer = io;
	svg.start_point = startp;
	svg.size = size;
	svg.default_font = "'MyriadPro-Regular'";
	svg.default_color = "#000000";
	setmetatable(svg,svg_meta);
	return svg;
end

function SVG:tostring()
	return "SVG object : width="..self.size.width.." height = "..self.size.height;
end

function SVG:open_file(output_file)
  if(output_file == "stdout") then
  	self.printer = io;
  else
  	self.printer = io.open(output_file,"w");
	end
end

function SVG:print(raw_text)
	self.printer:write(raw_text);
end

function SVG:finalize()
  if(self.printer ~= io) then
		self.printer:close();
  end
end

function SVG:print_header()
	startp = self.startp;
	size   = self.size;
	if(size == nil) then
			self.printer:write("");
	end
	if(startp == nil) then
		startp = {x = 0;y = 0};
	end
	self.printer:write( '<?xml version="1.0" encoding="utf-8"?>'.."\n"
  ..'<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN" "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">'.."\n"
  ..'<svg version="1.1" id="Calque_1" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" '
  ..'x="'..self.start_point.x..'px" y="'..self.start_point.y..'px"'.."\n"
  ..'	 width="'..self.size.width..'px" height="'..self.size.height..'px" '
  ..'viewBox="0 0 '..self.size.width..' '..self.size.height..'" '
  ..'enable-background="new 0 0 '..self.size.width..' '..self.size.height..'" xml:space="preserve">'.."\n");
end

function SVG:print_footer()
	self.printer:write('</svg>');
end

function SVG:group_begin(starting_point,style)
	if(style == nil) then
		style = "";
	end
	self.printer:write('<g transform="translate('..starting_point.x..','..starting_point.y..')" style="'..style..'">'.."\n");
end

function SVG:group_end()
	self.printer:write("</g>\n");
end

function SVG:draw_rect(starting_point,size,color)
  if(starting_point == nil) then
    starting_point = {x = 0,y = 0};
  end
  if(color == nil) then
    color = self.default_color;
  end  
  self.printer:write( 
  '<rect x="'..starting_point.x..'" y="'..starting_point.y..'" '
  ..'fill="none" stroke="'..color..'" stroke-miterlimit="10" '
  ..'width="'..size.width..'" height="'..size.height..'"/>'.."\n");
end

function SVG:draw_rrect(starting_point,size,color,id)
  local rounding = 12;
  if(starting_point == nil) then
    starting_point = {x = 0,y = 0};
  end
  if(color == nil) then
    color = self.default_color;
  end
  if(filling_color == nil) then
  	filling_color = "#ffffff";
  end  
  if(id == nil) then
    id = "";
  end  
  local width = size.width - rounding*2;
  local height = size.height - rounding*2;
  self.printer:write( 
  '<path fill="'..filling_color..'" ' 
  ..'stroke="'..color..'" '
  ..'stroke-miterlimit="10" '
  ..'id="'..id..'" ' 
  ..'d="m'..starting_point.x..','..starting_point.y+rounding
  ..'c 0 -6,5,-12,12,-12'
  ..'h '..width
  ..'c 6,0,12,5,12,12'
  ..'v '..height
  ..'c 0,6,-5,12,-12,12'
  ..'h -'..width
  ..'c -6,0,-12,-5,-12,-12'
  ..'v -'..height
  ..'z">'.."\n"
  ..'<animateColor fill="freeze" dur="0.1s" to="blue" from="yellow" attributeName="fill" begin="mouseover"/>'.."\n"
  ..'<animateColor fill="freeze" dur="0.1s" to="yellow" from="blue" attributeName="fill" begin="mouseout"/>'.."\n"
  ..'</path>'
  .."\n");
end

function SVG:draw_line(start_point,end_point,color,width)
  if(color == nil) then
    color = self.default_color;
  end
  if(width == nil) then
  	width = 1;
  end  
  self.printer:write(
  '<line fill="none" stroke="'..color..'" stroke-width="'..width..'" stroke-miterlimit="10" '
  ..'x1="'..start_point.x..'" y1="'..start_point.y..'" '
  ..'x2="'..end_point.x..'" y2="'..end_point.y..'"/>'..'\n');
end

function SVG:draw_circle(center_point,radius,color,fill_color)
  if(color == nil) then
    color = self.default_color;
  end
  if(fill_color == nil) then
  	fill_color = "none";
  end
  self.printer:write(
  '<circle cx="'..center_point.x..'" cy="'..center_point.y..'" '
  ..'r="'..radius..'" stroke="'..color..'" '
  ..'stroke-width="2" fill="'..fill_color..'"/>'..'\n');
end

function SVG:draw_point(point,thickness,color)
  if(color == nil) then
    color = self.default_color;
  end
  self:draw_circle(point,thickness,color,color);
end

function SVG:draw_text(text,font,size,start_point,color)
  if(text == nil) then
    text = "";
  end
  if(font == nil) then
    font = self.default_font;
  end 
  if(size == nil) then
    size = 12;
  end
  if(color == nil) then
    color = self.default_color;
  end   
  self.printer:write(
  '<text transform="matrix(1 0 0 1 '..start_point.x..' '..start_point.y..')" '
  ..'font-family="'..font..'" '
  ..'fill="'..color..'" '
  ..'font-size="'..size..'">'
  ..text..'</text>'..'\n');
end

function SVG:add_x_to_point(p,val)
	p.x = p.x + val;
	return p;
end

function SVG:add_y_to_point(p,val)
	p.y = p.y + val;
	return p;
end

function SVG:add_to_point(p,val)
	p.x = p.x + val;
	p.y = p.y + val;
	return p;
end

function SVG:sub_x_to_point(p,val)
	p.x = p.x - val;
	return p;
end

function SVG:sub_y_to_point(p,val)
	p.y = p.y - val;
	return p;
end

function SVG:sub_to_point(p,val)
	p.x = p.x - val;
	p.y = p.y - val;
	return p;
end

svg_meta.__tostring = SVG.tostring;
svg_meta.__index    = SVG;
svg_meta.name	    = "SVG";
