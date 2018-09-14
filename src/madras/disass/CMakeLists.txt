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

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src/madras) 
foreach(ARCH ${ARCHS})
   add_subdirectory(${ARCH})
endforeach(ARCH)

FILE(GLOB_RECURSE 
   file_disass
   dsmb_archinterface.c
   fsmutils.c
   libmdisass.c
   ${CMAKE_SOURCE_DIR}/src/madras/archinterface.c
   ${CMAKE_SOURCE_DIR}/src/madras/bfile_fmtinterface.c
)

foreach(ARCH ${ARCHS})
   SET(CMAKE_C_FLAGS          "${CMAKE_C_FLAGS} -D_ARCHDEF_${ARCH}")
endforeach(ARCH)

### --- Create the mdisass objects --- ###
# Create the mdisass object for STATIC libraries/binaries #
ADD_LIBRARY(mdisass-obj-static               OBJECT ${file_disass})
SET_TARGET_PROPERTIES(mdisass-obj-static     PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}")

# Create the mdisass object for DYNAMIC libraries/binaries #
ADD_LIBRARY(mdisass-obj-dyn                  OBJECT ${file_disass})
SET_TARGET_PROPERTIES(mdisass-obj-dyn        PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")

### --- Install the headers --- ###
FILE(COPY libmdisass.h           DESTINATION ${INCLUDE_OUTPUT_PATH})
FILE(COPY fsmutils.h             DESTINATION ${INCLUDE_OUTPUT_PATH})
FILE(COPY dsmb_archinterface.h   DESTINATION ${INCLUDE_OUTPUT_PATH})
