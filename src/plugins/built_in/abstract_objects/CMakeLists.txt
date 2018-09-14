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

add_subdirectory(stub)

SET(CURRENT_PATH "${CMAKE_SOURCE_DIR}/src/plugins/built_in/abstract_objects/")
SET(LUA_SCRIPTS_PATH ${CMAKE_SOURCE_DIR}/src/plugins/)

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/asmfile.lua.in ${CMAKE_CURRENT_SOURCE_DIR}/asmfile.lua @ONLY)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/arch.lua.in ${CMAKE_CURRENT_SOURCE_DIR}/arch.lua @ONLY)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/block.lua.in ${CMAKE_CURRENT_SOURCE_DIR}/block.lua @ONLY)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/fct.lua.in ${CMAKE_CURRENT_SOURCE_DIR}/fct.lua @ONLY)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/group.lua.in ${CMAKE_CURRENT_SOURCE_DIR}/group.lua @ONLY)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/insn.lua.in ${CMAKE_CURRENT_SOURCE_DIR}/insn.lua @ONLY)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/loop.lua.in ${CMAKE_CURRENT_SOURCE_DIR}/loop.lua @ONLY)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/patcher.lua.in ${CMAKE_CURRENT_SOURCE_DIR}/patcher.lua @ONLY)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/project.lua.in ${CMAKE_CURRENT_SOURCE_DIR}/project.lua @ONLY)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/tools.lua.in ${CMAKE_CURRENT_SOURCE_DIR}/tools.lua @ONLY)

ADD_CUSTOM_TARGET(distclean_abstractobjects 
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_SOURCE_DIR}/arch.lua
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_SOURCE_DIR}/asmfile.lua
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_SOURCE_DIR}/block.lua
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_SOURCE_DIR}/fct.lua
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_SOURCE_DIR}/group.lua
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_SOURCE_DIR}/insn.lua
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_SOURCE_DIR}/loop.lua
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_SOURCE_DIR}/patcher.lua
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_SOURCE_DIR}/project.lua
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_SOURCE_DIR}/tools.lua
    )
