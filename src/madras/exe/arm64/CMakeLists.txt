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

### --- Create the exts${SELECTED_ARCH}_empty library --- ###
# List the source files #
SET(source_files  ${CMAKE_CURRENT_SOURCE_DIR}/${SELECTED_ARCH}_finalfct.c)

# Create the library #
ADD_LIBRARY(exts${SELECTED_ARCH}_empty-lib-dyn              SHARED EXCLUDE_FROM_ALL ${source_files})
SET_TARGET_PROPERTIES(exts${SELECTED_ARCH}_empty-lib-dyn    PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")
