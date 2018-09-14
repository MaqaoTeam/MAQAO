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

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src/maqao/) 



### --- Create the abstract_objects_c_lua objects --- ###
# List the source files #
FILE(GLOB_RECURSE 
   source_files
   abstract_objects_c.c
   ao_project.c
   ao_asmfile.c
   ao_arch.c
   ao_function.c
   ao_group.c
   ao_loop.c
   ao_block.c
   ao_insn.c
   ao_insn_oprnd.c
)

# Create the abstract_objects_c_lua object for STATIC libraries/binaries #
ADD_LIBRARY(abstract_objects_c_lua-obj-static            OBJECT  ${source_files})
SET_TARGET_PROPERTIES(abstract_objects_c_lua-obj-static  PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}")
ADD_DEPENDENCIES(abstract_objects_c_lua-obj-static       ${DO_LUA_DEPENDENCY})

# Create the abstract_objects_c_lua object for DYNAMIC libraries/binaries #
ADD_LIBRARY(abstract_objects_c_lua-obj-dyn OBJECT        ${source_files})
SET_TARGET_PROPERTIES(abstract_objects_c_lua-obj-dyn     PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")
ADD_DEPENDENCIES(abstract_objects_c_lua-obj-dyn          ${DO_LUA_DEPENDENCY})



### --- Generate the dynamic abstract_objects_c_lua library --- ###
ADD_LIBRARY(abstract_objects_c_lua-lib-dyn               SHARED $<TARGET_OBJECTS:abstract_objects_c_lua-obj-dyn>)
SET_TARGET_PROPERTIES(abstract_objects_c_lua-lib-dyn     PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")
SET_TARGET_PROPERTIES(abstract_objects_c_lua-lib-dyn     PROPERTIES PREFIX "" OUTPUT_NAME abstract_objects_c)

# Link the dynamic abstract_objects_c_lua library to the required libraries #
TARGET_LINK_LIBRARIES(abstract_objects_c_lua-lib-dyn     maqao-lib-dyn)



### --- Install the headers --- ###
FILE(COPY abstract_objects_c.h DESTINATION ${INCLUDE_OUTPUT_PATH})

