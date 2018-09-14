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
	ADD_SUBDIRECTORY(${ARCH})
endforeach(ARCH)

FILE(GLOB_RECURSE 
   file_mdrs
   madras.c
   madras_help.c
)

FILE(GLOB_RECURSE
   file_mdrsmod
   madras.c
)

### --- Create the madras objects --- ###
# Create the madras object for static libraries #
ADD_LIBRARY(madras-module-static              OBJECT ${file_mdrsmod})
SET_TARGET_PROPERTIES(madras-module-static    PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}")
SET_TARGET_PROPERTIES(madras-module-static    PROPERTIES COMPILE_DEFINITIONS "STATIC_MODULE")

# Create the madras object for dynamic libraries #
ADD_LIBRARY(madras-module-dyn              OBJECT ${file_mdrsmod})
SET_TARGET_PROPERTIES(madras-module-dyn    PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")
SET_TARGET_PROPERTIES(madras-module-dyn    PROPERTIES COMPILE_DEFINITIONS "STATIC_MODULE")

# Create the madras object for static executables #
ADD_LIBRARY(madras-bin-static             OBJECT ${file_mdrs})
SET_TARGET_PROPERTIES(madras-bin-static   PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}")

# Create the madras object for dynamic executables #
ADD_LIBRARY(madras-bin-dyn                OBJECT ${file_mdrs})
SET_TARGET_PROPERTIES(madras-bin-dyn      PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")
