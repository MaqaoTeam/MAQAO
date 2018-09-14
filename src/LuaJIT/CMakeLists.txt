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

SET (LOCAL_LUA_LIB "${CMAKE_SOURCE_DIR}/src/LuaJIT/src/libluajit.a")
SET (LOCAL_LUA_BIN "${CMAKE_SOURCE_DIR}/src/LuaJIT/src/luajit")

SET (LUA_INCLUDE_OUTPUT_PATH "${INCLUDE_OUTPUT_PATH}/luajit")

IF (${CROSS_COMPILATION})
  SET(CC_DEVICE ${CMAKE_C_COMPILER})
  SET (i 1)
  WHILE (NOT "x${CMAKE_C_COMPILER_ARG${i}}" STREQUAL "x")
    SET (CC_DEVICE "${CC_DEVICE} ${CMAKE_C_COMPILER_ARG${i}}")
    MATH(EXPR i "${i} + 1")
  ENDWHILE()
  # (2017-07-25) Note: I have no idea why we need to add -fPIC to STATIC_CC, but there is a linker error otherwise.   
  ADD_CUSTOM_COMMAND(
	OUTPUT ${LUA_LIB_DYN}
	OUTPUT ${LUA_BIN}

	# Create device version of LuaJIT and copy it
	COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR} make 
		CC=${CC_DEVICE} HOST_CC=${CMAKE_C_COMPILER} XCFLAGS=-DLUAJIT_DISABLE_JIT STATIC_CC=${CC_DEVICE}\ -fPIC
	COMMAND ${CMAKE_COMMAND} -E copy ${LOCAL_LUA_LIB} ${LIBRARY_OUTPUT_PATH}
	COMMAND ${CMAKE_COMMAND} -E copy ${LOCAL_LUA_BIN} ${EXECUTABLE_OUTPUT_PATH}

	# Then create a host version of LuaJIT and keep it here
	COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR} make clean
	COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR} make
    )

ELSE ()
  ADD_CUSTOM_COMMAND(
   OUTPUT ${LUA_LIB_DYN}
	OUTPUT ${LUA_BIN}

	# Run the makefile
	COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR} make
	COMMAND ${CMAKE_COMMAND} -E copy ${LOCAL_LUA_LIB} ${LIBRARY_OUTPUT_PATH}
	COMMAND ${CMAKE_COMMAND} -E copy ${LOCAL_LUA_BIN} ${EXECUTABLE_OUTPUT_PATH}
    )
ENDIF ()

ADD_CUSTOM_TARGET(do_luajit DEPENDS ${LUA_LIB_DYN} ${LUA_BIN})
FILE(COPY src/lua.h     DESTINATION ${LUA_INCLUDE_OUTPUT_PATH})
FILE(COPY src/lualib.h  DESTINATION ${LUA_INCLUDE_OUTPUT_PATH})
FILE(COPY src/lua.hpp   DESTINATION ${LUA_INCLUDE_OUTPUT_PATH})
FILE(COPY src/lauxlib.h DESTINATION ${LUA_INCLUDE_OUTPUT_PATH})
FILE(COPY src/luaconf.h DESTINATION ${LUA_INCLUDE_OUTPUT_PATH})

ADD_CUSTOM_TARGET(distclean_luajit
         COMMAND make clean -C ${CMAKE_CURRENT_SOURCE_DIR}
         )

