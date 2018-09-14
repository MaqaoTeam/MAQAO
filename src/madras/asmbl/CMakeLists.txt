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
   file_asmb 
   asmb_archinterface.c
   asmblutils.c
   assembler.c
)

foreach(ARCH ${ARCHS})
   SET(CMAKE_C_FLAGS          "${CMAKE_C_FLAGS} -D_ARCHDEF_${ARCH}")
endforeach(ARCH)

### --- Create the masmbl objects --- ###
# Create the masmbl object for STATIC libraries/binaries #
ADD_LIBRARY(masmbl-obj-static                OBJECT ${file_asmb})
SET_TARGET_PROPERTIES(masmbl-obj-static      PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}")

# Create the masmbl object for DYNAMIC libraries/binaries #
ADD_LIBRARY(masmbl-obj-dyn                   OBJECT ${file_asmb})
SET_TARGET_PROPERTIES(masmbl-obj-dyn         PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")

### --- Install the headers --- ###
FILE(COPY assembler.h            DESTINATION ${INCLUDE_OUTPUT_PATH})
FILE(COPY asmb_archinterface.h   DESTINATION ${INCLUDE_OUTPUT_PATH})

