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

# If cross compilation is needed, generate new versions for fs and bitops
IF (${CROSS_COMPILATION})
  # Set compiler options
  IF (NOT "x${CC_HOST}" STREQUAL "x")
    SET (CMAKE_C_COMPILER ${CC_HOST})
    SET (i 1)
    WHILE (NOT "x${CMAKE_C_COMPILER_ARG${i}}" STREQUAL "x")
      SET (CMAKE_C_COMPILER_ARG${i} "")
      MATH(EXPR i "${i} + 1")
    ENDWHILE()
  ENDIF ()

  IF (NOT "x${CXX_HOST}" STREQUAL "x")
    SET (CMAKE_CXX_COMPILER ${CXX_HOST})
    SET (i 1)
    WHILE (NOT "x${CMAKE_CXX_COMPILER_ARG${i}}" STREQUAL "x")
      SET (CMAKE_CXX_COMPILER_ARG${i} "")
      MATH(EXPR i "${i} + 1")
    ENDWHILE()
  ENDIF ()

  # Set other options
  SET(LIBRARY_OUTPUT_PATH    "${CMAKE_SOURCE_DIR}/src/maqao")

  # Rebuild fs
  FILE(GLOB 
    files_fs
    ${CMAKE_CURRENT_SOURCE_DIR}/../fs/fs.c
  )
  ADD_LIBRARY(cc_fs_lua SHARED  ${files_fs})
  ADD_DEPENDENCIES (cc_fs_lua ${DO_LUA_DEPENDENCY})
  SET_TARGET_PROPERTIES(cc_fs_lua PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")
  SET_TARGET_PROPERTIES(cc_fs_lua PROPERTIES PREFIX "" OUTPUT_NAME fs)

  # Rebuild lfs
  FILE(GLOB 
    files_lfs
    ${CMAKE_CURRENT_SOURCE_DIR}/../luafilesystem-1.7.0/src/lfs.c
  )
  ADD_LIBRARY(cc_lfs_lua SHARED  ${files_lfs})
  ADD_DEPENDENCIES (cc_lfs_lua ${DO_LUA_DEPENDENCY})
  SET_TARGET_PROPERTIES(cc_lfs_lua PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")
  SET_TARGET_PROPERTIES(cc_lfs_lua PROPERTIES PREFIX "" OUTPUT_NAME lfs)

  # Rebuild bitops
  FILE(GLOB
    files_bitops
    ${CMAKE_CURRENT_SOURCE_DIR}/../bitops/bitops.c
  )
  ADD_LIBRARY(cc_bitops_lua SHARED  ${files_bitops})
  ADD_DEPENDENCIES (cc_bitops_lua ${DO_LUA_DEPENDENCY})
  SET_TARGET_PROPERTIES(cc_bitops_lua PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")
  SET_TARGET_PROPERTIES(cc_bitops_lua PROPERTIES PREFIX "" OUTPUT_NAME bitops)

# else, just copy existing files in src/maqao
ELSE ()
  ADD_CUSTOM_TARGET (cc_fs_lua
      COMMAND ${CMAKE_COMMAND} -E copy 
	  $<TARGET_FILE:fs_lua-lib-dyn>
	  ${CMAKE_SOURCE_DIR}/src/maqao
      DEPENDS fs_lua-lib-dyn
  )

  ADD_CUSTOM_TARGET (cc_lfs_lua
      COMMAND ${CMAKE_COMMAND} -E copy 
	  $<TARGET_FILE:lfs_lua-lib-dyn>
	  ${CMAKE_SOURCE_DIR}/src/maqao
      DEPENDS lfs_lua-lib-dyn
  )

  ADD_CUSTOM_TARGET (cc_bitops_lua
      COMMAND ${CMAKE_COMMAND} -E copy 
	  $<TARGET_FILE:bitops_lua-lib-dyn>
	  ${CMAKE_SOURCE_DIR}/src/maqao
      DEPENDS bitops_lua-lib-dyn
  )
  
ENDIF ()

ADD_CUSTOM_TARGET(distclean_crosscompil
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_SOURCE_DIR}/src/maqao/fs.so #$<TARGET_FILE_NAME:fs_lua-lib-dyn>
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_SOURCE_DIR}/src/maqao/lfs.so #$<TARGET_FILE_NAME:lfs_lua-lib-dyn>
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_SOURCE_DIR}/src/maqao/bitops.so #$<TARGET_FILE_NAME:bitops_lua-lib-dyn>
    VERBATIM
    )

