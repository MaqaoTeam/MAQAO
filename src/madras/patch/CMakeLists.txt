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

foreach(ARCH ${ARCHS})
   add_subdirectory(${ARCH})
endforeach(ARCH)

FILE(GLOB_RECURSE 
   file_patch
   patch_archinterface.c
   libmpatch.c
   patchutils.c
)

foreach(ARCH ${ARCHS})
   SET(CMAKE_C_FLAGS          "${CMAKE_C_FLAGS} -D_ARCHDEF_${ARCH}")
endforeach(ARCH)

### --- Create the mpatch objects --- ###
# Create the mpatch object for STATIC libraries/binaries #
ADD_LIBRARY(mpatch-obj-static                OBJECT ${file_patch})
SET_TARGET_PROPERTIES(mpatch-obj-static      PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}")

# Create the mpatch object for DYNAMIC libraries/binaries #
ADD_LIBRARY(mpatch-obj-dyn                   OBJECT ${file_patch})
SET_TARGET_PROPERTIES(mpatch-obj-dyn         PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")

### --- Install the headers --- ###
FILE(COPY libmpatch.h            DESTINATION ${INCLUDE_OUTPUT_PATH})
FILE(COPY patchutils.h           DESTINATION ${INCLUDE_OUTPUT_PATH})
FILE(COPY patch_archinterface.h  DESTINATION ${INCLUDE_OUTPUT_PATH})
