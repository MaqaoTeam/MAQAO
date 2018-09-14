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

SET(LIBRARY_OUTPUT_PATH    "${CMAKE_CURRENT_SOURCE_DIR}")



### --- Create the fs_lua objects --- ###
# List the source files #
FILE(GLOB_RECURSE 
   source_files
   fs.c
)
# Create the fs_lua object for STATIC libraries/binaries #
ADD_LIBRARY(fs_lua-obj-static             OBJECT  ${source_files})
SET_TARGET_PROPERTIES(fs_lua-obj-static   PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}")

# Create the fs_lua object for DYNAMIC libraries/binaries #
ADD_LIBRARY(fs_lua-obj-dyn                OBJECT  ${source_files})
SET_TARGET_PROPERTIES(fs_lua-obj-dyn      PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")


### --- Generate the dynamic fs_lua library --- ###
ADD_LIBRARY(fs_lua-lib-dyn SHARED         $<TARGET_OBJECTS:fs_lua-obj-dyn> $<TARGET_OBJECTS:mcommon-obj-dyn>)
SET_TARGET_PROPERTIES(fs_lua-lib-dyn      PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")
SET_TARGET_PROPERTIES(fs_lua-lib-dyn      PROPERTIES PREFIX "" OUTPUT_NAME fs)

# Link the dynamic fs_lua library to the required libraries #
TARGET_LINK_LIBRARIES(fs_lua-lib-dyn      ${LUA_LIB_DYN})
ADD_DEPENDENCIES(fs_lua-lib-dyn           ${DO_LUA_DEPENDENCY})
