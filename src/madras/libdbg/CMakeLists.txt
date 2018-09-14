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

add_subdirectory(libdwarf)

FILE(GLOB_RECURSE 
   file_dwarfreader
   DwarfLight.c
)

FILE(GLOB_RECURSE
   file_dbg
   libmdbg.c
)

foreach(ARCH ${ARCHS})
   FILE(GLOB source_files     ${CMAKE_CURRENT_SOURCE_DIR}/${ARCH}/${ARCH}_DwarfLight.c) 
   SET(file_dwarfreader       "${file_dwarfreader};${source_files}")
endforeach(ARCH)

### --- Create the dwarfreader objects --- ###
# Create the dwarfreader object for STATIC libraries/binaries #
ADD_LIBRARY(dwarfreader-obj-static           OBJECT ${file_dwarfreader})
SET_TARGET_PROPERTIES(dwarfreader-obj-static PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}")

# Create the dwarfreader object for DYNAMIC libraries/binaries #
ADD_LIBRARY(dwarfreader-obj-dyn              OBJECT ${file_dwarfreader})
SET_TARGET_PROPERTIES(dwarfreader-obj-dyn    PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")

### --- Create the dwarfreader objects --- ###
# Create the mdbg object for STATIC libraries/binaries #
ADD_LIBRARY(mdbg-obj-static                  OBJECT ${file_dbg})
SET_TARGET_PROPERTIES(mdbg-obj-static        PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}")

# Create the mdbg object for DYNAMIC libraries/binaries #
ADD_LIBRARY(mdbg-obj-dyn                     OBJECT ${file_dbg})
SET_TARGET_PROPERTIES(mdbg-obj-dyn           PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")

### --- Install the headers --- ###
FILE(COPY libmdbg.h              DESTINATION ${INCLUDE_OUTPUT_PATH})
FILE(COPY DwarfLight.h           DESTINATION ${INCLUDE_OUTPUT_PATH})
