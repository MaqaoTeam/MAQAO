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

# ---- Compile content
foreach(ARCH ${ARCHS})
   ADD_SUBDIRECTORY(${ARCH})
   INCLUDE_DIRECTORIES(${ARCH})
endforeach(ARCH)

### --- Generate the mmaqao objects --- ###
# List the source files #
FILE(GLOB_RECURSE 
   source_libmmaqao
   libmmaqao.c
   lua_exec.c
   modules_wrapper.c
   uarch_detector.c
)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})

# Create the mmaqao object for STATIC libraries/binaries #
ADD_LIBRARY(mmaqao-obj-static                   OBJECT ${source_libmmaqao})
ADD_DEPENDENCIES(mmaqao-obj-static do_dwarf     ${DO_LUA_DEPENDENCY} do_luastatic)
SET_TARGET_PROPERTIES(mmaqao-obj-static         PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}")

# Create the mmaqao object for DYNAMIC libraries/binaries #
ADD_LIBRARY(mmaqao-obj-dyn                      OBJECT ${source_libmmaqao})
ADD_DEPENDENCIES(mmaqao-obj-dyn do_dwarf        ${DO_LUA_DEPENDENCY} do_luastatic)
SET_TARGET_PROPERTIES(mmaqao-obj-dyn            PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")



### --- Generate the uarch_detector objects --- ###

# Create the uarch_detector object for STATIC libraries/binaries #
ADD_LIBRARY(uarch_detector-obj-static           OBJECT  uarch_detector.c)
SET_TARGET_PROPERTIES(uarch_detector-obj-static PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}")

# Create the uarch_detector object for DYNAMIC libraries/binaries #
ADD_LIBRARY(uarch_detector-obj-dyn              OBJECT  uarch_detector.c)
SET_TARGET_PROPERTIES(uarch_detector-obj-dyn    PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")

# ---- Declared used architectures for constant extraction
foreach(ARCH ${ARCHS})
   SET (is_${ARCH} true)
endforeach(ARCH)

# ---- Configure files
CONFIGURE_FILE(cst_C2Lua.lua.in        ${CMAKE_CURRENT_SOURCE_DIR}/cst_C2Lua.lua @ONLY)
CONFIGURE_FILE(load_images.lua.in      ${CMAKE_CURRENT_SOURCE_DIR}/load_images.lua @ONLY)
CONFIGURE_FILE(lua_static.lua.in       ${CMAKE_CURRENT_SOURCE_DIR}/lua_static.lua @ONLY)
CONFIGURE_FILE(errs2Lua.lua.in         ${CMAKE_CURRENT_SOURCE_DIR}/errs2Lua.lua @ONLY)

ADD_CUSTOM_TARGET(do_luastatic 
                  COMMAND ${LUA_BIN_HOST} ${CMAKE_CURRENT_SOURCE_DIR}/cst_C2Lua.lua
                  COMMAND ${LUA_BIN_HOST} ${CMAKE_CURRENT_SOURCE_DIR}/errs2Lua.lua 
                  COMMAND ${LUA_BIN_HOST} ${CMAKE_CURRENT_SOURCE_DIR}/load_images.lua
                  COMMAND ${LUA_BIN_HOST} ${CMAKE_CURRENT_SOURCE_DIR}/lua_static.lua 
                               DEPENDS ${DO_LUA_DEPENDENCY} cc_lfs_lua cc_fs_lua cc_bitops_lua)

ADD_CUSTOM_TARGET(distclean_maqao DEPENDS distclean_maqao_LuaConsts
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_SOURCE_DIR}/cst_C2Lua.lua
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_SOURCE_DIR}/load_images.lua
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_SOURCE_DIR}/lua_static.lua
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_SOURCE_DIR}/errs2Lua.lua
    VERBATIM
    )

