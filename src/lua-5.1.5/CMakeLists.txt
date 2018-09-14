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

SET (LOCAL_LUA_LIB "${CMAKE_SOURCE_DIR}/src/lua-5.1.5/src/liblua.a")
SET (LOCAL_LUA_BIN "${CMAKE_SOURCE_DIR}/src/lua-5.1.5/src/lua")

SET (LUA_INCLUDE_OUTPUT_PATH "${INCLUDE_OUTPUT_PATH}/lua")



IF (${CROSS_COMPILATION})
  SET(CC_DEVICE ${CMAKE_C_COMPILER})
  SET (i 1)
  WHILE (NOT "x${CMAKE_C_COMPILER_ARG${i}}" STREQUAL "x")
    SET (CC_DEVICE "${CC_DEVICE} ${CMAKE_C_COMPILER_ARG${i}}")
    MATH(EXPR i "${i} + 1")
  ENDWHILE()

  ADD_CUSTOM_COMMAND(
        OUTPUT ${LUA_LIB_STATIC}
        OUTPUT ${LUA_LIB_DYN}
        OUTPUT ${LUA_BIN}

        # Create device version of Lua and copy it
        COMMAND ${CMAKE_COMMAND} -E echo 
                "Compiling Lua with make linux CC=${CC_DEVICE}"
	COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR} 
		make posix CC=${CC_DEVICE} 
        COMMAND ${CMAKE_COMMAND} -E copy ${LOCAL_LUA_LIB} ${LIBRARY_OUTPUT_PATH}
        COMMAND ${CMAKE_COMMAND} -E copy ${LOCAL_LUA_BIN} ${EXECUTABLE_OUTPUT_PATH}

        # Then create a host version of Lua and keep it here
        COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR} 
                make clean
        COMMAND ${CMAKE_COMMAND} -E echo
                "Compiling Lua with make linux"
        COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR} 
                make linux
  )

ELSE ()
  # Select the command according to the current OS (linux / macosx)
  IF (APPLE)
    ADD_CUSTOM_COMMAND(
      OUTPUT ${LUA_LIB_STATIC}
      OUTPUT ${LUA_LIB_DYN}
      OUTPUT ${LUA_BIN}

      # Run the makefile
      COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR} make macosx
      COMMAND ${CMAKE_COMMAND} -E copy ${LOCAL_LUA_LIB} ${LIBRARY_OUTPUT_PATH}
      COMMAND ${CMAKE_COMMAND} -E copy ${LOCAL_LUA_BIN} ${EXECUTABLE_OUTPUT_PATH}
    )
  ENDIF ()
  IF (UNIX)
    ADD_CUSTOM_COMMAND(
      OUTPUT ${LUA_LIB_STATIC}
      OUTPUT ${LUA_BIN}

      # Run the makefile
      COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR} make linux
      COMMAND ${CMAKE_COMMAND} -E copy ${LOCAL_LUA_LIB} ${LIBRARY_OUTPUT_PATH}
      COMMAND ${CMAKE_COMMAND} -E copy ${LOCAL_LUA_BIN} ${EXECUTABLE_OUTPUT_PATH}
    )
  ENDIF ()
  IF (is_WINDOWS)
    ADD_SUBDIRECTORY(src)
    ADD_CUSTOM_COMMAND(
      OUTPUT ${LUA_LIB_STATIC}
      OUTPUT ${LUA_LIB_DYN}
      OUTPUT ${LUA_BIN}

      
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:lua-lib-dyn> ${LIBRARY_OUTPUT_PATH}
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:lua-lib-dyn> "${CMAKE_CURRENT_SOURCE_DIR}/src/"
      COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE_DIR:lua-lib-dyn>/lua51.lib" ${LIBRARY_OUTPUT_PATH}
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:lua-lib-static> ${LIBRARY_OUTPUT_PATH}
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:lua> ${EXECUTABLE_OUTPUT_PATH}
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:lua> "${CMAKE_CURRENT_SOURCE_DIR}/src/"
      DEPENDS lua-lib-dyn lua-lib-static lua
    )
  ENDIF ()
ENDIF ()

IF (UNIX)
  ADD_CUSTOM_TARGET(do_lua DEPENDS ${LUA_LIB_STATIC} ${LUA_BIN})
ELSE ()
  ADD_CUSTOM_TARGET(do_lua DEPENDS ${LUA_LIB_DYN} ${LUA_LIB_STATIC} ${LUA_BIN})
ENDIF ()
FILE(COPY src/lua.h     DESTINATION ${LUA_INCLUDE_OUTPUT_PATH})
FILE(COPY src/lualib.h  DESTINATION ${LUA_INCLUDE_OUTPUT_PATH})
FILE(COPY src/lauxlib.h DESTINATION ${LUA_INCLUDE_OUTPUT_PATH})
FILE(COPY src/luaconf.h DESTINATION ${LUA_INCLUDE_OUTPUT_PATH})

ADD_CUSTOM_TARGET(distclean_lua
         COMMAND make clean -C ${CMAKE_CURRENT_SOURCE_DIR}
         )
