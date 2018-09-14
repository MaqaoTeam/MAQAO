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

SET (SELECTED_ARCH arm64) 
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src/madras/patch) 
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src/asm/${SELECTED_ARCH}) 

### ===========================================================================
### --- Create the mpatch objects --- ###
# List the source files #
SET (file_patch   ${CMAKE_CURRENT_SOURCE_DIR}/${SELECTED_ARCH}_patcher.c)

# Create the mpatch object for STATIC libraries/binaries #
ADD_LIBRARY(mpatch${SELECTED_ARCH}-obj-static            OBJECT  ${file_patch})
SET_TARGET_PROPERTIES(mpatch${SELECTED_ARCH}-obj-static  PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}")

IF (is_WINDOWS)
   # Create the mpatch object for DYNAMIC libraries/binaries #
   ADD_LIBRARY(mpatch${SELECTED_ARCH}-obj-dyn               OBJECT  ${file_patch})
   SET_TARGET_PROPERTIES(mpatch${SELECTED_ARCH}-obj-dyn     PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")
ENDIF ()
