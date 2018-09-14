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

FILE(GLOB_RECURSE 
   files_asm
   la*.c
   la*.cpp
)

foreach(ARCH ${ARCHS})
   add_subdirectory(${ARCH})
endforeach(ARCH)

### --- Create the masm objects --- ###
# Create the masm object for STATIC libraries/binaries #
ADD_LIBRARY(masm-obj-static               OBJECT ${files_asm})
SET_TARGET_PROPERTIES(masm-obj-static     PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}")

# Create the masm object for DYNAMIC libraries/binaries #
ADD_LIBRARY(masm-obj-dyn                  OBJECT ${files_asm})
SET_TARGET_PROPERTIES(masm-obj-dyn        PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")

IF (IS_STDCXX)
   SET_TARGET_PROPERTIES (masm-obj-static PROPERTIES COMPILE_DEFINITIONS "IS_STDCXX")
   SET_TARGET_PROPERTIES (masm-obj-dyn    PROPERTIES COMPILE_DEFINITIONS "IS_STDCXX")
   MESSAGE ("-- stdc++ available")
ENDIF (IS_STDCXX)

### --- Install the headers --- ###
FILE(COPY libmasm.h DESTINATION     ${INCLUDE_OUTPUT_PATH})

