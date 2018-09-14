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
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src/madras/disass) 
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src/asm/${SELECTED_ARCH}) 

### ===========================================================================
### --- Create the mdsmbl objects --- ###
# List the source files #
SET (file_disass  ${CMAKE_CURRENT_SOURCE_DIR}/${SELECTED_ARCH}_fct_0.c
                  ${CMAKE_CURRENT_SOURCE_DIR}/${SELECTED_ARCH}_fct_1.c
                  ${CMAKE_CURRENT_SOURCE_DIR}/${SELECTED_ARCH}_fct_2.c
                  ${CMAKE_CURRENT_SOURCE_DIR}/${SELECTED_ARCH}_fct_3.c
                  ${CMAKE_CURRENT_SOURCE_DIR}/${SELECTED_ARCH}_fct_4.c
                  ${CMAKE_CURRENT_SOURCE_DIR}/${SELECTED_ARCH}_fct_5.c
                  ${CMAKE_CURRENT_SOURCE_DIR}/${SELECTED_ARCH}_fct_6.c
                  ${CMAKE_CURRENT_SOURCE_DIR}/${SELECTED_ARCH}_fct_7.c
                  ${CMAKE_CURRENT_SOURCE_DIR}/${SELECTED_ARCH}_fct_8.c
                  ${CMAKE_CURRENT_SOURCE_DIR}/${SELECTED_ARCH}_fct_9.c
                  ${CMAKE_CURRENT_SOURCE_DIR}/${SELECTED_ARCH}_fct_a.c
                  ${CMAKE_CURRENT_SOURCE_DIR}/${SELECTED_ARCH}_fct_b.c
                  ${CMAKE_CURRENT_SOURCE_DIR}/${SELECTED_ARCH}_fct_c.c
                  ${CMAKE_CURRENT_SOURCE_DIR}/${SELECTED_ARCH}_fct_d.c
                  ${CMAKE_CURRENT_SOURCE_DIR}/${SELECTED_ARCH}_fct_e.c
                  ${CMAKE_CURRENT_SOURCE_DIR}/${SELECTED_ARCH}_fct_f.c
                  ${CMAKE_CURRENT_SOURCE_DIR}/${SELECTED_ARCH}_fsm.c
                  ${CMAKE_CURRENT_SOURCE_DIR}/${SELECTED_ARCH}_macros.c)
                  
# Create the mdsmbl object for STATIC libraries/binaries #
ADD_LIBRARY(mdsmbl${SELECTED_ARCH}-obj-static            OBJECT  ${file_disass})
SET_TARGET_PROPERTIES(mdsmbl${SELECTED_ARCH}-obj-static  PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}")

IF (is_WINDOWS)
   # Create the mdsmbl object for DYNAMIC libraries/binaries #
   ADD_LIBRARY(mdsmbl${SELECTED_ARCH}-obj-dyn               OBJECT  ${file_disass})
   SET_TARGET_PROPERTIES(mdsmbl${SELECTED_ARCH}-obj-dyn     PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")
ENDIF ()
