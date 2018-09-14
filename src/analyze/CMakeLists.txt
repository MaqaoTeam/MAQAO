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


INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src/maqao) 

### --- List source files --- ###
# List architecture specific source files #
SET(core_arch_files "")
foreach(ARCH ${ARCHS})
	FILE(GLOB arch_files ${CMAKE_CURRENT_SOURCE_DIR}/${ARCH}/*.c) 
	SET(core_arch_files "${core_arch_files};${arch_files}")
endforeach(ARCH)

# List common source files #
FILE(GLOB 
   core_files
   ${CMAKE_CURRENT_SOURCE_DIR}/*.c
)



### --- Create the manalyze objects --- ###
# Create the manalyze object for STATIC libraries/binaries #
ADD_LIBRARY(manalyze-obj-static              OBJECT ${core_files} ${core_arch_files})
SET_TARGET_PROPERTIES(manalyze-obj-static    PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}") 

# Create the manalyze object for DYNAMIC libraries/binaries #
ADD_LIBRARY(manalyze-obj-dyn                 OBJECT ${core_files} ${core_arch_files})
SET_TARGET_PROPERTIES(manalyze-obj-dyn       PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")

# Add definitions flags #
SET(COMPILE_DEF "")
foreach(ARCH ${ARCHS})
	SET(COMPILE_DEF "${COMPILE_DEF};_ARCHDEF_${ARCH}")
endforeach(ARCH)
SET_TARGET_PROPERTIES (manalyze-obj-static   PROPERTIES COMPILE_DEFINITIONS "${COMPILE_DEF}")
SET_TARGET_PROPERTIES (manalyze-obj-dyn      PROPERTIES COMPILE_DEFINITIONS "${COMPILE_DEF}")



### --- Install the headers --- ###
FILE(COPY libmcore.h DESTINATION ${INCLUDE_OUTPUT_PATH})

