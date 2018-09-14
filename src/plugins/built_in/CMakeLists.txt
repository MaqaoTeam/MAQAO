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

IF (NOT is_WINDOWS)
	add_subdirectory(fcgx)
ENDIF ()

add_subdirectory(fs)
add_subdirectory(madras)
add_subdirectory(abstract_objects)
add_subdirectory(classes)
add_subdirectory(bitops)
add_subdirectory(common)
add_subdirectory(errcode)
add_subdirectory(luafilesystem-1.7.0)
add_subdirectory(cross_compilation)

SET(LUA_SCRIPTS_PATH ${CMAKE_SOURCE_DIR}/src/plugins/)
SET(LOCAL_PORT "8080")

# Declaring architecture-related constants
foreach(ARCH ${ARCHS})
    # Flag signaling an architecture is set    
	SET (is_${ARCH} true)
	# Lua declarations of all flags signaling if the corresponding architecture is set
    SET (LUA_ARCH_DECLS "${LUA_ARCH_DECLS}local is_${ARCH} = \"true\";\n")
    # For Consts.lua.in: declaration of the inclusion of the files containing architecture specific variables
    SET (CONSTS_ARCHS_INCLUDES "${CONSTS_ARCHS_INCLUDES}Consts.${ARCH} = {};\ndofile(\"${LUA_SCRIPTS_PATH}/built_in/classes/Consts/${ARCH}.lua\");\ndofile(\"${LUA_SCRIPTS_PATH}/built_in/classes/Consts/${ARCH}_c.lua\");\n")
endforeach(ARCH)

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/classes/Help.lua.in 
               ${CMAKE_CURRENT_SOURCE_DIR}/classes/Help.lua @ONLY)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/classes/Consts.lua.in 
               ${CMAKE_CURRENT_SOURCE_DIR}/classes/Consts.lua @ONLY)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/abstract_objects.lua.in 
               ${CMAKE_CURRENT_SOURCE_DIR}/abstract_objects.lua @ONLY)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/classes.lua.in 
               ${CMAKE_CURRENT_SOURCE_DIR}/classes.lua @ONLY)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/classes/MDSAPI.lua.in 
               ${CMAKE_CURRENT_SOURCE_DIR}/classes/MDSAPI.lua @ONLY)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/classes/Utils.lua.in 
               ${CMAKE_CURRENT_SOURCE_DIR}/classes/Utils.lua @ONLY)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/classes/XLSX.lua.in 
               ${CMAKE_CURRENT_SOURCE_DIR}/classes/XLSX.lua @ONLY)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/classes/HTML.lua.in 
               ${CMAKE_CURRENT_SOURCE_DIR}/classes/HTML.lua @ONLY)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/classes/TEXT.lua.in 
               ${CMAKE_CURRENT_SOURCE_DIR}/classes/TEXT.lua @ONLY)

ADD_CUSTOM_TARGET(distclean_builtin 
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_SOURCE_DIR}/classes/Help.lua
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_SOURCE_DIR}/classes/Consts.lua
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_SOURCE_DIR}/abstract_objects.lua
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_SOURCE_DIR}/classes.lua
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_SOURCE_DIR}/classes/MDSAPI.lua
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_SOURCE_DIR}/classes/Utils.lua
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_SOURCE_DIR}/classes/XLSX.lua
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_SOURCE_DIR}/classes/HTML.lua
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_SOURCE_DIR}/classes/TEXT.lua

    VERBATIM
    )
