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

### ===========================================================================
### --- Create the march objects --- ###
# List the source files #
SET (file_arch    ${CMAKE_CURRENT_SOURCE_DIR}/${SELECTED_ARCH}_arch.c
                  ${CMAKE_CURRENT_SOURCE_DIR}/${SELECTED_ARCH}_implicit_reg.c
                  ${CMAKE_CURRENT_SOURCE_DIR}/${SELECTED_ARCH}_uarch.c)
                  
# Create the march object for STATIC libraries/binaries #
ADD_LIBRARY(march${SELECTED_ARCH}-obj-static             OBJECT  ${file_arch})
SET_TARGET_PROPERTIES(march${SELECTED_ARCH}-obj-static   PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}")


IF (is_WINDOWS)
# Create the march object for DYNAMIC libraries/binaries #
    ADD_LIBRARY(march${SELECTED_ARCH}-obj-dyn                OBJECT  ${file_arch})
    SET_TARGET_PROPERTIES(march${SELECTED_ARCH}-obj-dyn      PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")
ENDIF ()


### ===========================================================================
### --- Create the mext objects --- ###
# List the source files #
SET (file_ext     ${CMAKE_CURRENT_SOURCE_DIR}/${SELECTED_ARCH}_ext.c)

# Create the mext object for STATIC libraries/binaries # 
ADD_LIBRARY(mext${SELECTED_ARCH}-obj-static              OBJECT  ${file_ext})
SET_TARGET_PROPERTIES(mext${SELECTED_ARCH}-obj-static    PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}")

IF (is_WINDOWS)
    # Create the mext object for DYNAMIC libraries/binaries #
    ADD_LIBRARY(mext${SELECTED_ARCH}-obj-dyn                 OBJECT  ${file_ext})
    SET_TARGET_PROPERTIES(mext${SELECTED_ARCH}-obj-dyn       PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")
ENDIF ()


### --- Install the headers --- ###
FILE(COPY ${SELECTED_ARCH}_arch.h DESTINATION ${INCLUDE_OUTPUT_PATH})
FILE(COPY ${SELECTED_ARCH}_uarch.h DESTINATION ${INCLUDE_OUTPUT_PATH})
FILE(COPY ${SELECTED_ARCH}_ext.h DESTINATION ${INCLUDE_OUTPUT_PATH})
