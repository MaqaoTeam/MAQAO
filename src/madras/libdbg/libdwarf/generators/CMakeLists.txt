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

SET(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/..")
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/..) 

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

ADD_EXECUTABLE (gennames gennames.c dwgetopt.c)
SET_TARGET_PROPERTIES(gennames PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${EXECUTABLE_OUTPUT_PATH}")
SET_TARGET_PROPERTIES(gennames PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG "${EXECUTABLE_OUTPUT_PATH}")
SET_TARGET_PROPERTIES(gennames PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE "${EXECUTABLE_OUTPUT_PATH}")
msvc_posix(gennames)
