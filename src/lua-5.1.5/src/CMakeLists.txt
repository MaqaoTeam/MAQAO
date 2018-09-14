##
#   Copyright (C) 2004 - 2018 Université de Versailles Saint-Quentin-en-Yvelines (UVSQ)
#
#   This file is part of MAQAO.
#
#  MAQAO is free software; you can redistribute it and/or
#   modify it under the terms of the GNU Lesser General Public License
#   as published by the Free Software Foundation; either version 3
#   of the License, or (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU Lesser General Public License for more details.
#
#   You should have received a copy of the GNU Lesser General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
##

SET(LIBRARY_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
SET(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

### --- Create the lua objects --- ###
# List the source files #
FILE(GLOB_RECURSE 
   source_files
   lapi.c
   lauxlib.c
   lbaselib.c
   lcode.c
   ldblib.c
   ldebug.c
   ldo.c
   ldump.c
   lfunc.c
   lgc.c
   linit.c
   liolib.c
   llex.c
   lmathlib.c
   lmem.c
   loadlib.c
   lobject.c
   lopcodes.c
   loslib.c
   lparser.c
   lstate.c
   lstring.c
   lstrlib.c
   ltable.c
   ltablib.c
   ltm.c
   lundump.c
   lvm.c
   lzio.c
   print.c
)

# Create the lua object for STATIC libraries/binaries #
ADD_LIBRARY(lua-obj-static                OBJECT  ${source_files})
SET_TARGET_PROPERTIES(lua-obj-static      PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}")

# Create the lua object for DYNAMIC libraries/binaries #
ADD_LIBRARY(lua-obj-dyn                   OBJECT  ${source_files})
SET_TARGET_PROPERTIES(lua-obj-dyn         PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")
SET_TARGET_PROPERTIES(lua-obj-dyn         PROPERTIES COMPILE_DEFINITIONS "LUA_BUILD_AS_DLL")



### --- Generate the dynamic lua library --- ###
ADD_LIBRARY(lua-lib-dyn SHARED            $<TARGET_OBJECTS:lua-obj-dyn>)
SET_TARGET_PROPERTIES(lua-lib-dyn         PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")
SET_TARGET_PROPERTIES(lua-lib-dyn         PROPERTIES COMPILE_DEFINITIONS "LUA_BUILD_AS_DLL")
SET_TARGET_PROPERTIES(lua-lib-dyn         PROPERTIES PREFIX "" OUTPUT_NAME lua51)



### --- Generate the static lua library --- ###
ADD_LIBRARY(lua-lib-static STATIC            $<TARGET_OBJECTS:lua-obj-static>)
SET_TARGET_PROPERTIES(lua-lib-static      PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}")
SET_TARGET_PROPERTIES(lua-lib-static      PROPERTIES PREFIX "" OUTPUT_NAME lua51static)



### --- Generate the dynamic lua executable --- ###
ADD_EXECUTABLE(lua                        lua.c)
SET_TARGET_PROPERTIES(lua                 PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")

# Link the dynamic lua executable to the required libraries #
TARGET_LINK_LIBRARIES(lua                 lua-lib-dyn)



### --- Generate the static luac executable --- ###
ADD_EXECUTABLE(luac                       ${source_files} print.c)
SET_TARGET_PROPERTIES(luac                PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}")