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

### --- Create the lprof instrumentation libraries --- ###
# List the source files #
FILE(GLOB_RECURSE 
   files
   libinstru.c
   probes_price.c
)


### --- Create the dynamic lprof instrumentation library --- ###
# Create the library #
ADD_LIBRARY(instru-lib-dyn                      SHARED ${files})
SET_TARGET_PROPERTIES(instru-lib-dyn            PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")
SET_TARGET_PROPERTIES(instru-lib-dyn            PROPERTIES OUTPUT_NAME instru)

# Link the dynamic lprof instrumentation library to the required libraries #
TARGET_LINK_LIBRARIES(instru-lib-dyn            rt)

# Install the dynamic lprof instrumentation library #
INSTALL(TARGETS instru-lib-dyn                  DESTINATION lib)


### --- Create the dynamic lprof gnu openmp instrumentation library --- ###
# Create the library #
ADD_LIBRARY(instru-gomp-lib-dyn                 SHARED ${files})
SET_TARGET_PROPERTIES(instru-gomp-lib-dyn       PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")
SET_TARGET_PROPERTIES(instru-gomp-lib-dyn       PROPERTIES COMPILE_FLAGS "-fopenmp")
SET_TARGET_PROPERTIES(instru-gomp-lib-dyn       PROPERTIES COMPILE_DEFINITIONS "OPENMP")
SET_TARGET_PROPERTIES(instru-gomp-lib-dyn       PROPERTIES PREFIX "" OUTPUT_NAME libinstru-gomp)

# Link the dynamic lprof gnu openmp instrumentation library to the required libraries #
TARGET_LINK_LIBRARIES(instru-gomp-lib-dyn       rt)

# Install the dynamic lprof gnu openmp instrumentation library #
INSTALL(TARGETS instru-gomp-lib-dyn             DESTINATION lib)


### --- Create the dynamic lprof intel openmp instrumentation library --- ###
FIND_LIBRARY(LIOMP5_DYN_PATH iomp5 PATHS ENV LD_LIBRARY_PATH)
IF (NOT (${LIOMP5_DYN_PATH} STREQUAL "LIOMP5_DYN_PATH-NOTFOUND"))  
   # Create the library #
   ADD_LIBRARY(instru-iomp-lib-dyn              SHARED ${files})
   SET_TARGET_PROPERTIES(instru-iomp-lib-dyn    PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")
   SET_TARGET_PROPERTIES(instru-iomp-lib-dyn    PROPERTIES COMPILE_DEFINITIONS "OPENMP")
   SET_TARGET_PROPERTIES(instru-iomp-lib-dyn    PROPERTIES PREFIX "" OUTPUT_NAME libinstru-iomp)
   
   # Link the dynamic lprof intel openmp instrumentation library to the required libraries #
   TARGET_LINK_LIBRARIES(instru-iomp-lib-dyn    ${LIOMP5_PATH} rt)
   
   # Install the dynamic lprof intel openmp instrumentation library #
   INSTALL(TARGETS instru-iomp-lib-dyn          DESTINATION lib)
   
ELSE ()
   MESSAGE("WARNING: could not generate Intel OPENMP version of libinstru. Please load Intel environment.")
ENDIF ()


### --- Create the static lprof instrumentation library --- ###
# Create the library #
ADD_LIBRARY(instru-lib-static                   STATIC ${files})
SET_TARGET_PROPERTIES(instru-lib-static         PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}")
SET_TARGET_PROPERTIES(instru-lib-static         PROPERTIES PREFIX "" OUTPUT_NAME libinstru)

# Link the static lprof instrumentation library to the required libraries #
TARGET_LINK_LIBRARIES(instru-lib-static rt)


### --- Create the static lprof gnu openmp instrumentation library --- ###
# Create the library #
ADD_LIBRARY(instru-gomp-lib-static              STATIC ${files})
SET_TARGET_PROPERTIES(instru-gomp-lib-static    PROPERTIES COMPILE_FLAGS "-fopenmp")
SET_TARGET_PROPERTIES(instru-gomp-lib-static    PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}")
SET_TARGET_PROPERTIES(instru-gomp-lib-static    PROPERTIES COMPILE_DEFINITIONS "OPENMP")
SET_TARGET_PROPERTIES(instru-gomp-lib-static    PROPERTIES PREFIX "" OUTPUT_NAME libinstru-gomp)

# Link the static lprof gnu openmp instrumentation library to the required libraries #
TARGET_LINK_LIBRARIES(instru-gomp-lib-static    rt)


### --- Create the static lprof intel openmp instrumentation library --- ###
FIND_LIBRARY(LIOMP5_STA_PATH libiomp5.a PATHS ENV LD_LIBRARY_PATH)
IF (NOT (${LIOMP5_STA_PATH} STREQUAL "LIOMP5_STA_PATH-NOTFOUND"))  
   # Create the library #
   ADD_LIBRARY(instru-iomp-lib-static           STATIC ${files})
   SET_TARGET_PROPERTIES(instru-iomp-lib-static PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}")
   SET_TARGET_PROPERTIES(instru-iomp-lib-static PROPERTIES COMPILE_DEFINITIONS "OPENMP")
   SET_TARGET_PROPERTIES(instru-iomp-lib-static PROPERTIES PREFIX "" OUTPUT_NAME libinstru-iomp)  
   
   # Link the static lprof intel openmp instrumentation library to the required libraries #
   TARGET_LINK_LIBRARIES(instru-iomp-lib-static ${LIOMP5_PATH} rt)
   
ELSE ()
   MESSAGE("WARNING: could not generate Intel OPENMP version of libinstru (static). Please load Intel environment.")
ENDIF ()


