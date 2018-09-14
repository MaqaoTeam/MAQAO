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

add_subdirectory(disass)
add_subdirectory(asmbl)
add_subdirectory(patch)
add_subdirectory(libtroll)
add_subdirectory(libdbg)
add_subdirectory(exe)
add_subdirectory(libworm)
add_subdirectory(libstone)

FILE(GLOB_RECURSE 
   file_libmdrs
   libmadras.c
)

### --- Create the madras objects --- ###
# Create the madras object for static libraries/binaries #
ADD_LIBRARY(madras-obj-static             OBJECT ${file_libmdrs})
SET_TARGET_PROPERTIES(madras-obj-static   PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}")

# Create the madras object for dynamic libraries/binaries #
ADD_LIBRARY(madras-obj-dyn                OBJECT ${file_libmdrs})
SET_TARGET_PROPERTIES(madras-obj-dyn      PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")

### --- Install headers --- ###
FILE(COPY libmadras.h  DESTINATION ${INCLUDE_OUTPUT_PATH})
FILE(COPY archinterface.h  DESTINATION ${INCLUDE_OUTPUT_PATH})
