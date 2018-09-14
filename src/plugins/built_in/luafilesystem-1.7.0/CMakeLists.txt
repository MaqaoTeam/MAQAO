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

SET(LIBRARY_OUTPUT_PATH    "${CMAKE_CURRENT_SOURCE_DIR}/src")

SET (PREFIX ${CMAKE_SOURCE_DIR})
#CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/src/plugins/built_in/luafilesystem-1.7.0/config.in 
#               ${CMAKE_SOURCE_DIR}/src/plugins/built_in/luafilesystem-1.7.0/config @ONLY)



### --- Create the lfs_lua objects --- ###
# List the source files #
FILE(GLOB_RECURSE 
   source_files
   src/*.c
)

# Create the lfs_lua object for STATIC libraries/binaries #
ADD_LIBRARY(lfs_lua-obj-static            OBJECT  ${source_files})
SET_TARGET_PROPERTIES(lfs_lua-obj-static  PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}")

# Create the lfs_lua object for DYNAMIC libraries/binaries #
ADD_LIBRARY(lfs_lua-obj-dyn               OBJECT  ${source_files})
SET_TARGET_PROPERTIES(lfs_lua-obj-dyn     PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")


### --- Generate the dynamic lfs_lua library --- ###
ADD_LIBRARY(lfs_lua-lib-dyn               SHARED $<TARGET_OBJECTS:lfs_lua-obj-dyn>)
SET_TARGET_PROPERTIES(lfs_lua-lib-dyn     PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")
SET_TARGET_PROPERTIES(lfs_lua-lib-dyn     PROPERTIES PREFIX "" OUTPUT_NAME lfs)

# Link the dynamic lfs_lua library to the required libraries #
TARGET_LINK_LIBRARIES(lfs_lua-lib-dyn     ${LUA_LIB_DYN})
ADD_DEPENDENCIES(lfs_lua-lib-dyn          ${DO_LUA_DEPENDENCY})
