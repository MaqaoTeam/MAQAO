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

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src/maqao/)

### --- Create the lprof sampling objects --- ###
# List the source files #
FILE(GLOB 
   files
   list_libc.c
   lprof_c.c
   perf_util.c
   libcounting.c
   utils.c
   binary_format.c
   IP_events_format.c
   avltree.c
   sampling_engine.c
   sampling_engine_inherit.c
   sampling_engine_ptrace.c
   sampling_engine_timers.c
   sampling_engine_shared.c
   sampling_engine_data_struct.c
   sampling_engine_dump_collect_data.c
   generate_metafile.c
   generate_metafile_shared.c
   prepare_sampling_display.c
   prepare_sampling_display_shared.c
   deprecated_shared.c
   unwind.c
)

# Required by libunwind
IF ("X${DISABLE_LIBUNWIND}" STREQUAL "Xfalse" OR "X${DISABLE_LIBUNWIND}" STREQUAL "X")
  # Create a copy of a CMake variable before modifying it
  SET(CPY_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
  SET(CMAKE_FIND_LIBRARY_SUFFIXES .a)

  FIND_LIBRARY(LIBUNWIND_PATH unwind)
  FIND_LIBRARY(LIBUNWIND_GENERIC_PATH unwind-generic)
  FIND_LIBRARY(LIBLZMA_GENERIC_PATH lzma)

  # Restore the CMake variable
  SET(CMAKE_FIND_LIBRARY_SUFFIXES ${CPY_CMAKE_FIND_LIBRARY_SUFFIXES})

  SET (use_libunwind TRUE)
  IF ("${LIBUNWIND_PATH}" STREQUAL "LIBUNWIND_PATH-NOTFOUND")
    MESSAGE ("#### libunwind.a is missing ####")
    SET (use_libunwind FALSE)
  ENDIF ()
  IF ("${LIBUNWIND_GENERIC_PATH}" STREQUAL "LIBUNWIND_GENERIC_PATH-NOTFOUND")
    MESSAGE ("#### libunwind-generic.a is missing ####")
    SET (use_libunwind FALSE)
  ENDIF ()
  IF ("${LIBLZMA_GENERIC_PATH}" STREQUAL "LIBLZMA_GENERIC_PATH-NOTFOUND")
    MESSAGE ("#### liblzma.a is missing ####")
    SET (use_libunwind FALSE)
  ENDIF ()
ELSEIF ("X${DISABLE_LIBUNWIND}" STREQUAL "Xtrue")
  SET (use_libunwind FALSE)
ENDIF ()

# Add libunwind dependencies to MAQAO executable and dynamic library
IF (${use_libunwind})
  MESSAGE ("LINKING WITH LIBUNINWID")
  SET (LIBS_SUPP ${LIBS_SUPP} unwind-generic unwind lzma CACHE INTERNAL "Extra libraries needed by plugins")
ELSE ()
  MESSAGE ("LINKING WITHOUT LIBUNWIND")
ENDIF ()


# Create the lprof object for STATIC libraries/binaries #
ADD_LIBRARY(lprof_c_lua-obj-static              OBJECT  ${files})
SET_TARGET_PROPERTIES(lprof_c_lua-obj-static    PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}")
IF (${use_libunwind})
  SET_TARGET_PROPERTIES(lprof_c_lua-obj-static PROPERTIES COMPILE_FLAGS "-D__LIBUNWIND__")
ENDIF ()

# Create the lprof object for DYNAMIC libraries/binaries #
ADD_LIBRARY(lprof_c_lua-obj-dyn                 OBJECT  ${files})
SET_TARGET_PROPERTIES(lprof_c_lua-obj-dyn       PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")
IF (${use_libunwind})
  SET_TARGET_PROPERTIES(lprof_c_lua-obj-dyn PROPERTIES COMPILE_FLAGS "-D__LIBUNWIND__")
ENDIF ()


### --- Create the static lprof_c_lua sampling library --- ###
# Create the library #
ADD_LIBRARY(lprof_c_lua-lib-static              STATIC $<TARGET_OBJECTS:lprof_c_lua-obj-static> )
SET_TARGET_PROPERTIES(lprof_c_lua-lib-static    PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}")
SET_TARGET_PROPERTIES(lprof_c_lua-lib-static    PROPERTIES PREFIX "" OUTPUT_NAME lprof_c)


### --- Create the dynamic lprof_c_lua sampling library --- ###
# Create the library #
ADD_LIBRARY(lprof_c_lua-lib-dyn                 SHARED  $<TARGET_OBJECTS:lprof_c_lua-obj-dyn>)
SET_TARGET_PROPERTIES(lprof_c_lua-lib-dyn       PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")
SET_TARGET_PROPERTIES(lprof_c_lua-lib-dyn       PROPERTIES PREFIX "" OUTPUT_NAME lprof_c)
ADD_DEPENDENCIES (lprof_c_lua-lib-dyn           maqao-lib-dyn abstract_objects_c_lua-lib-dyn)

# Link the dynamic lprof_c_lua library to the required libraries #
TARGET_LINK_LIBRARIES(lprof_c_lua-lib-dyn       maqao-lib-dyn m pthread abstract_objects_c_lua-lib-dyn)


### --- Create the libcounting library --- ###
# List the source files #
FILE(GLOB
   mylib
   utils.c
   perf_util.c
   binary_format.c
   uarch_detector.c
   libcounting.c
   deprecated_shared.c
)

# Create the library #
ADD_LIBRARY(lprof-lib-dyn SHARED                ${mylib}
                                                $<TARGET_OBJECTS:mcommon-obj-dyn> 
                                                $<TARGET_OBJECTS:uarch_detector-obj-dyn>)
                                             SET_TARGET_PROPERTIES(lprof-lib-dyn             PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}" OUTPUT_NAME lprof)

# Link the dynamic libcounting library to the required libraries #
TARGET_LINK_LIBRARIES(lprof-lib-dyn             m)


### --- Install the headers --- ###
FILE(COPY perf_event.h DESTINATION ${INCLUDE_OUTPUT_PATH}/)
FILE(COPY libcounting.h DESTINATION ${INCLUDE_OUTPUT_PATH}/)
FILE(COPY avltree.h DESTINATION ${INCLUDE_OUTPUT_PATH}/)
