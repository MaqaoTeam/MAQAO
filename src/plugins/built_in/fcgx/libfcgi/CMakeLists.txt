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

### --- Create the fcgi objects --- ###
# List the source files #
FILE(GLOB_RECURSE 
   source_files
   fcgiapp.c
   fcgi_stdio.c
   os_unix.c 
)

# Create the fcgi object for STATIC libraries/binaries #
ADD_LIBRARY(fcgi-obj-static            OBJECT ${source_files})
SET_TARGET_PROPERTIES(fcgi-obj-static  PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}")

# Create the fcgi object for DYNAMIC libraries/binaries #
ADD_LIBRARY(fcgi-obj-dyn               OBJECT ${source_files})
SET_TARGET_PROPERTIES(fcgi-obj-dyn     PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")



### --- Install the headers --- ###
FILE(COPY fcgiapp.h DESTINATION ${INCLUDE_OUTPUT_PATH})
