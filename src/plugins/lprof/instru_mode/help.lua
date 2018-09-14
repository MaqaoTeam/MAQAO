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

module ("lprof.instru_mode.help", package.seeall)

function add_collect (help)
   help:add_separator("   INSTRUMENTATION MODE");

   help:add_option ("instrumentation-profile=", "ip=", nil, false,
                    "Selects target instrumentation profile:\n"..
                    "<.br>  - FI : Measures inclusive time spent by functions\n"..
                    "<.br>  - FX : Measures exclusive and inclusive time spent by functions\n"..
                    "<.br>  - LO : Measures time spent in outermost loops\n"..
                    "<.br>  - LI : Measures time spent in innermost loops\n"
   );

   help:add_option ( "instrumentation-model= [OPTIONAL]", "im=", nil, false,
                 "Selects the model of the application:\n"..
                 "<.br> - Unicore  : One core and one thread (default)\n"..
                 "<.br> - IOMP     : Support for Intel OpenMP runtimes\n"..
                 "<.br> - GOMP     : Support for GNU OpenMP runtimes"
   );

   help:add_option ("instrumentation-filter-function= [OPTIONAL]", "iff=", nil, false,
                  "Measures a list of functions provided by the user in this format : foo,bar,main,...\n"
   );
end

function add_display (help)
   help:add_separator("   INSTRUMENTATION MODE");
   help:add_option ("display-type= [OPTIONAL]", "dt=", nil, false,
                  " - txt : raw text format (default)\n"..
                  "<.br> - dot : Dot graph format\n"..
                  "<.br> - png : Portable Network Graphics image format\n"
   );

   help:add_option ("filter-thread= [OPTIONAL]", "ft=", nil, false,
                  "Selects the threads using the given list of OpenMP ids (e.g: 0,1,8,...).\n"
   );
end

function add_examples (help)
   --INSTRUMENTATION MODE EXAMPLES
   help:add_example ("maqao lprof im=IOMP ip=FX xp=<EXPERIMENT_DIRECTORY> <APPLICATION>",
                     "Selects FX profile with OpenMP (icc). Store results \n"..
                     "<.br>and instrumented binary in the specified <EXPERIMENT_DIRECTORY>.\n"..
                     "<.br>Intel OpenMP runtime is selected as the runtime model.\n");

   help:add_example ("maqao lprof ip=FX -dt=png -xp=<EXPERIMENT_DIRECTORY> ",
                     "Displays results from the previous output path as an image.\n");

   help:add_example ("maqao lprof -ip=FX -dt=txt -xp=<EXPERIMENT_DIRECTORY> ",
                     "Displays results from the previous output path as a text.");
end
