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

CMAKE_MINIMUM_REQUIRED(VERSION 2.8.12)
PROJECT(MAQAO)
ENABLE_LANGUAGE(C)
INCLUDE(InstallRequiredSystemLibraries)

## ------------------------------------------------------------------##
##                        Platform detection                         ##
## ------------------------------------------------------------------##

SET (is_UNIX false)
SET (is_APPLE false)
SET (is_WINDOWS false)

IF(WIN32 OR CYGWIN)
   SET (is_WINDOWS true)
ELSEIF(APPLE)
   SET (is_APPLE true)
ELSEIF(UNIX)
   SET (is_UNIX true)
ENDIF()

## ------------------------------------------------------------------##
##                       Parameters handling                         ##
## ------------------------------------------------------------------##

# Use to load the profile through -DPROFILE=<val>
IF( "X${PROFILE}" STREQUAL "X" )
   SET(PROFILE "default")
ENDIF()
IF(EXISTS ${CMAKE_SOURCE_DIR}/profiles/${PROFILE}.profile)
   MESSAGE ("-- Use ${PROFILE} profile")
   INCLUDE ("${CMAKE_SOURCE_DIR}/profiles/${PROFILE}.profile")
ELSE ()
   MESSAGE (FATAL_ERROR "Profile ${CMAKE_SOURCE_DIR}/profiles/${PROFILE}.profile doesn't exist")
ENDIF ()

## ------------------------------------------------------------------##
##                Handling multi-architecture flags                  ##
## ------------------------------------------------------------------##

# Multi-architecture handling
# Use -DARCHS="arch1;arch2;arch3" to support arch1 arch2 and arch3
IF ("X${ARCHS}" STREQUAL "X")
   MESSAGE("-- Using default value (arm64) for ARCHS")
   SET(ARCHS "arm64")
ENDIF ()

FOREACH(ARCH ${ARCHS})
   SET(ARCHDEF_FLAGS "${ARCHDEF_FLAGS} -D_ARCHDEF_${ARCH}")
ENDFOREACH(ARCH)

## ------------------------------------------------------------------##
##                Handling uarch exclusions                          ##
## ------------------------------------------------------------------##
IF (NOT "X${EXCLUDE_UARCHS}" STREQUAL "X")
    FOREACH(EXCLUDE_UARCH ${EXCLUDE_UARCHS})
	MESSAGE("Excluding uarch ${EXCLUDE_UARCH}")
        SET(UARCH_EXCLUDE_FLAGS "${UARCH_EXCLUDE_FLAGS} -DUARCH_EXCLUDE_${EXCLUDE_UARCH}")   
    ENDFOREACH(EXCLUDE_UARCH)

ENDIF (NOT "X${EXCLUDE_UARCHS}" STREQUAL "X")

## ------------------------------------------------------------------##
##               Handling modules exclusions                         ##
## ------------------------------------------------------------------##

# List the source plugins 
FILE(GLOB
   plugins_sources
   ${CMAKE_CURRENT_SOURCE_DIR}/src/plugins/*
)

FOREACH(file ${plugins_sources})
   IF(NOT "${file}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}/src/plugins/built_in"
   AND IS_DIRECTORY "${file}"
   AND EXISTS "${file}/CMakeLists.txt")
   
      # Get the last part of "file"
      GET_FILENAME_COMPONENT(file ${file} NAME)
      
      # If EXCLUDE_MODULES is not empty
      IF (NOT "x${EXCLUDE_MODULES}" STREQUAL "x")
      
         # Check if the module is excluded or not
         IF ("${EXCLUDE_MODULES}" MATCHES "^${file}$"
         OR  "${EXCLUDE_MODULES}" MATCHES "^${file};"
         OR  "${EXCLUDE_MODULES}" MATCHES ";${file}$"
         OR  "${EXCLUDE_MODULES}" MATCHES ";${file};")
            # Here the module is excluded
         ELSE ()
            # Here the module is included
            SET(${file}_enabled true)
         ENDIF ()
         
      # EXCLUDE_MODULES is empty, but INCLUDE_MODULES is not empty
      ELSEIF (NOT "${INCLUDE_MODULES}" STREQUAL "")
      
         # Check if the module is included or not
         IF ("${INCLUDE_MODULES}" MATCHES "^${file}$"
         OR  "${INCLUDE_MODULES}" MATCHES "^${file};"
         OR  "${INCLUDE_MODULES}" MATCHES ";${file}$"
         OR  "${INCLUDE_MODULES}" MATCHES ";${file};")
            # Here the module is included
            SET(${file}_enabled true)
         ENDIF ()
         
      # No rule specified, include the module
      ELSE ()
         # Here the module is included
         SET(${file}_enabled true)
      ENDIF()
   ENDIF()
ENDFOREACH(file)

## ------------------------------------------------------------------##
##                        Compilation flags                          ##
## ------------------------------------------------------------------##

SET(BASE_C_FLAGS "")
MESSAGE("-- Using C compiler ${CMAKE_C_COMPILER}")

# Use DEBUG variable to activate or not debug macros for MAQAO
IF ("x${DEBUG}" STREQUAL "xtrue")
   SET(DBG_OPTS      "")
   SET(CMAKE_BUILD_TYPE Debug)
ELSE ()
   SET(CMAKE_BUILD_TYPE Release)
   SET(DBG_OPTS      "-DNDEBUG")
ENDIF ()

# Set a default value for the optimization level
IF ("x${OPTIM_LVL}" STREQUAL "x")
   SET(OPTIM_LVL "2")
ENDIF ()

MESSAGE("-- Optimisation level: ${OPTIM_LVL}")

# Sets the optimisation level
IF (is_UNIX)
   SET(OPTIM_LEVEL   "-O${OPTIM_LVL}")
ELSEIF(is_WINDOWS)
   SET(OPTIM_LEVEL   "/O${OPTIM_LVL}")
ENDIF ()

# Add platform specific flags
IF (is_WINDOWS)
   SET(BASE_C_FLAGS  "${BASE_C_FLAGS} /Qstd=c99")
ENDIF ()

# Add warning flags
IF (is_WINDOWS)
   SET(BASE_C_FLAGS  "${BASE_C_FLAGS} /Wall")
ELSEIF (is_UNIX)
   SET(BASE_C_FLAGS  "${BASE_C_FLAGS} -Wall")
ENDIF ()

# Remove default Windows runtime library used by CMake...
IF (is_WINDOWS)
   FOREACH (variable 	CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE 
                        CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO 
                        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE 
                        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
      IF (${variable} MATCHES "/MDd")
         STRING(REGEX REPLACE "/MDd" "" ${variable} "${${variable}}")
      ENDIF ()
      IF (${variable} MATCHES "/MD")
         STRING(REGEX REPLACE "/MD" "" ${variable} "${${variable}}")
      ENDIF ()
   ENDFOREACH ()
ENDIF ()

# Initializing C/C++ compilation flags
IF (is_WINDOWS) 
   # C flags
   SET(CMAKE_C_FLAGS                      "${CMAKE_C_FLAGS}${BASE_C_FLAGS} ${OPTIM_LEVEL} ${DBG_OPTS} ${ARCHDEF_FLAGS} ${UARCH_EXCLUDE_FLAGS}")
   SET(CMAKE_C_FLAGS_DEBUG                "${CMAKE_C_FLAGS_DEBUG} /Zi")
   SET(CMAKE_C_FLAGS_RELEASE              "${CMAKE_C_FLAGS_RELEASE}")
   # C++ flags
   SET(CMAKE_CXX_FLAGS                    "${CMAKE_CXX_FLAGS}${BASE_C_FLAGS} ${OPTIM_LEVEL} ${DBG_OPTS} ${ARCHDEF_FLAGS} ${UARCH_EXCLUDE_FLAGS}")
   SET(CMAKE_CXX_FLAGS_DEBUG              "${CMAKE_CXX_FLAGS_DEBUG} /Zi")
   SET(CMAKE_CXX_FLAGS_RELEASE            "${CMAKE_CXX_FLAGS_RELEASE}")
   # Static C flags (USER MADE ! YOU NEED TO INCLUDE IT WHEN REQUIRED)
   SET(C_STATIC_FLAGS                     "")
   SET(C_STATIC_FLAGS_DEBUG               "/MTd")
   SET(C_STATIC_FLAGS_RELEASE             "/MT")
   # Dynamic C flags (USER MADE ! YOU NEED TO INCLUDE IT WHEN REQUIRED)
   SET(C_DYNAMIC_FLAGS                    "")
   SET(C_DYNAMIC_FLAGS_DEBUG              "/MDd")
   SET(C_DYNAMIC_FLAGS_RELEASE            "/MD")
   # Executable linker flags
   SET(CMAKE_EXE_LINKER_FLAGS             "${CMAKE_EXE_LINKER_FLAGS}")
ELSEIF (is_UNIX)
   # C flags
   SET(CMAKE_C_FLAGS                      "${CMAKE_C_FLAGS} ${BASE_C_FLAGS} ${OPTIM_LEVEL} ${DBG_OPTS} ${ARCHDEF_FLAGS}  ${UARCH_EXCLUDE_FLAGS} -fPIC")
   SET(CMAKE_C_FLAGS_DEBUG                "-W -g")
   SET(CMAKE_C_FLAGS_RELEASE              "")
   # C++ flags
   SET(CMAKE_CXX_FLAGS                    "${CMAKE_CXX_FLAGS} ${BASE_C_FLAGS} ${OPTIM_LEVEL} ${DBG_OPTS} ${ARCHDEF_FLAGS}  ${UARCH_EXCLUDE_FLAGS} -fPIC")
   SET(CMAKE_CXX_FLAGS_DEBUG              "-W -g")
   SET(CMAKE_CXX_FLAGS_RELEASE            "")
   # Static C flags (USER MADE ! YOU NEED TO INCLUDE IT WHEN REQUIRED)
   SET(C_STATIC_FLAGS                     "-static")
   SET(C_STATIC_FLAGS_DEBUG               "")
   SET(C_STATIC_FLAGS_RELEASE             "")
   # Dynamic C flags (USER MADE ! YOU NEED TO INCLUDE IT WHEN REQUIRED)
   SET(C_DYNAMIC_FLAGS                    "")
   SET(C_DYNAMIC_FLAGS_DEBUG              "")
   SET(C_DYNAMIC_FLAGS_RELEASE            "")
   # Executable linker flags
   SET(CMAKE_EXE_LINKER_FLAGS             "-Wl,--no-as-needed ${CMAKE_EXE_LINKER_FLAGS}")
ENDIF ()

# Building C_STATIC/DYNAMIC_FLAGS for debug and release builds
IF (CMAKE_BUILD_TYPE MATCHES "Debug")
   SET(C_STATIC_FLAGS   "${C_STATIC_FLAGS} ${C_STATIC_FLAGS_DEBUG}")
   SET(C_DYNAMIC_FLAGS  "${C_DYNAMIC_FLAGS} ${C_DYNAMIC_FLAGS_DEBUG}")
ELSE ()
   SET(C_STATIC_FLAGS   "${C_STATIC_FLAGS} ${C_STATIC_FLAGS_RELEASE}")
   SET(C_DYNAMIC_FLAGS 	"${C_DYNAMIC_FLAGS} ${C_DYNAMIC_FLAGS_RELEASE}")
ENDIF ()

## ------------------------------------------------------------------##
##                             Strip                                 ##
## ------------------------------------------------------------------##

# Strip
# Can be set to true / false using -DSTRIP=<val>
IF ("X${STRIP}" STREQUAL "Xfalse")
   SET(STRIP false)
ELSE ("X${STRIP}" STREQUAL "Xfalse")
   SET(STRIP true)
ENDIF ()

## ------------------------------------------------------------------##
##                    Handling lua dependencies                      ##
## ------------------------------------------------------------------##

# Can be set to lua / luajit using -DLUA=<val>
# Default is luajit, TODO: lua is safer, isn't it ?
IF( "X${LUA}" STREQUAL "Xlua" )
   SET (LUA_INCLUDE_OUTPUT_PATH  "${CMAKE_SOURCE_DIR}/include/lua")
   IF (is_WINDOWS)
      SET (LUA_LIB_DYN           lua-lib-dyn)
      SET (LUA_LIB_STATIC        lua-lib-static)
      SET (LUA_BIN               "${CMAKE_SOURCE_DIR}/bin/lua.exe")
      SET (LUA_BIN_HOST          "${CMAKE_SOURCE_DIR}/src/lua-5.1.5/src/lua.exe")
   ELSE ()
      SET (LUA_LIB_DYN           "${CMAKE_SOURCE_DIR}/lib/liblua.a")
      SET (LUA_LIB_STATIC        "${CMAKE_SOURCE_DIR}/lib/liblua.a")
      SET (LUA_BIN               "${CMAKE_SOURCE_DIR}/bin/lua")
      SET (LUA_BIN_HOST          "${CMAKE_SOURCE_DIR}/src/lua-5.1.5/src/lua")
   ENDIF ()
   SET (DO_LUA_DEPENDENCY        do_lua)
   SET (LUA_DIR                  "lua-5.1.5")
   SET (DISTCLEAN_LUA            distclean_lua)
ELSE ()
   SET (LUA_INCLUDE_OUTPUT_PATH  "${CMAKE_SOURCE_DIR}/include/luajit")
   SET (LUA_BIN                  "${CMAKE_SOURCE_DIR}/bin/luajit")
   SET (LUA_LIB_DYN              "${CMAKE_SOURCE_DIR}/lib/libluajit.a")
   SET (LUA_LIB_STATIC           "${CMAKE_SOURCE_DIR}/lib/libluajit.a")
   SET (LUA_BIN_HOST             "${CMAKE_SOURCE_DIR}/src/LuaJIT/src/luajit")
   SET (LUA_LIB_HOST             "${CMAKE_SOURCE_DIR}/src/LuaJIT/src/libluajit.a")
   SET (DO_LUA_DEPENDENCY        do_luajit)
   SET (LUA_DIR                  "LuaJIT")
   SET (DISTCLEAN_LUA            distclean_luajit)
ENDIF ()

SET(VERSION_HASH_UNKNOWN "Build information not available")
IF( "X${VERSION_HASH}" STREQUAL "X" )
   SET(VERSION_HASH "${VERSION_HASH_UNKNOWN}")
ENDIF ()

MESSAGE ("-- Uses ${LUA_DIR}")

## ------------------------------------------------------------------##
##                       Handling libstdc++                          ##
## ------------------------------------------------------------------##
# If defined, should contains the full path to libstdc++.a
SET (STDCXX_PATH )

SET (STDCXX stdc++)

IF ("${CMAKE_C_COMPILER}" MATCHES "/cc"
OR  "${CMAKE_C_COMPILER}" MATCHES "/gcc"
OR ((NOT "x${STDCXX_PATH}" STREQUAL "x") AND EXISTS ${STDCXX_PATH}))
   SET (IS_STDCXX true)
   IF ((NOT "x${STDCXX_PATH}" STREQUAL "x") AND (EXISTS ${STDCXX_PATH}))
      MESSAGE ("-- Using custom value for stdc++: ${STDCXX_PATH}")
      SET (STDCXX ${STDCXX_PATH})
   ENDIF()
ENDIF ()

## ------------------------------------------------------------------##
##                         Package generation                        ##
## ------------------------------------------------------------------##
#SET(CMAKE_BUILD_TYPE Release)
find_path(RPMBUILD_PATH NAMES rpmbuild)   # look for rpmbuild
find_path(DPKG_DEB_PATH NAMES dpkg-deb)   # look for dpkg-deb

SET(CPACK_PACKAGE_VERSION_MAJOR "2")
SET(CPACK_PACKAGE_VERSION_MINOR "5")
SET(CPACK_PACKAGE_VERSION_PATCH "9")

# ---- Check if at least one package generator has been found
IF ((NOT (${RPMBUILD_PATH} STREQUAL "RPMBUILD_PATH-NOTFOUND"))
OR(NOT (${DPKG_DEB_PATH} STREQUAL "DPKG_DEB_PATH-NOTFOUND")))
   # ---- Configure the package
   SET(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
   SET(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README")
   SET(CPACK_PACKAGE_NAME "maqao")
   SET(CPACK_PACKAGE_CONTACT "contact@maqao.org")
   SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "MAQAO is a performance tuning tool")
   SET(CPACK_GENERATOR "TBZ2")
   #SET(CPACK_SOURCE_GENERATOR "TBZ2")

   # ---- Generate debian package if posssible
   IF (NOT (${DPKG_DEB_PATH} STREQUAL "DPKG_DEB_PATH-NOTFOUND"))
      SET(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
      SET(CPACK_GENERATOR "DEB;${CPACK_GENERATOR}")
   ENDIF ()
   
   # ---- Generate RPM package if possible
   IF(NOT (${RPMBUILD_PATH} STREQUAL "RPMBUILD_PATH-NOTFOUND"))
      SET(CPACK_RPM_PACKAGE_ARCHITECTURE "arm64") 
      SET(CPACK_GENERATOR "RPM;${CPACK_GENERATOR}")
   ENDIF ()
   INCLUDE(CPack)
ENDIF ()

## ------------------------------------------------------------------##
##                         Check dependencies                        ##
## ------------------------------------------------------------------##
INCLUDE (CheckIncludeFiles) 
INCLUDE (CheckLibraryExists)
INCLUDE (CheckTypeSize) 

# missing checks: libc.a, libstdc++.a

# ---- Zip (optionnal - ONE-View XLSX)
FIND_PROGRAM (ZIP_BIN NAMES zip DOC "zip binary")
IF ("${ZIP_BIN}" STREQUAL "ZIP_BIN-NOTFOUND")
   MESSAGE (WARNING "zip not found. .xlsx files can not be generated")
   SET(IS_ZIP false)
ELSE ()
   MESSAGE (STATUS "zip found")
   SET(IS_ZIP true)
ENDIF ()

# ---- Doxygen (optionnal)
FIND_PROGRAM(DOXYGEN_BIN NAMES doxygen DOC "doxygen path")
IF ("${DOXYGEN_BIN}" STREQUAL "DOXYGEN_BIN-NOTFOUND")
   MESSAGE (WARNING "Doxygen not found. The documentation will not be generated")
   SET(IS_DOXYGEN false)
ELSE ("${DOXYGEN_BIN}" STREQUAL "DOXYGEN_BIN-NOTFOUND")
   MESSAGE (STATUS "Doxygen found in ${DOXYGEN_BIN}")
   SET(IS_DOXYGEN true)
ENDIF ()

# ---- Luadoc (optionnal)
FIND_PROGRAM(LUADOC_BIN NAMES luadoc luadoc.lua ldoc DOC "luadoc path")
IF ("${LUADOC_BIN}" STREQUAL "LUADOC_BIN-NOTFOUND")
   MESSAGE (WARNING "Luadoc not found. The documentation will not be generated")
   SET(IS_LUADOC false)
ELSE ("${LUADOC_BIN}" STREQUAL "LUADOC_BIN-NOTFOUND")
   MESSAGE (STATUS "Luadoc found in ${LUADOC_BIN}")
   SET(IS_LUADOC true)
ENDIF ()


# ---- Static libraries needed by MAQAO
IF (is_UNIX)
   # Create a copy of a CMake variable before modifying it
   SET(CPY_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
   SET(CMAKE_FIND_LIBRARY_SUFFIXES .a)
   
   # Get libraries
   FIND_LIBRARY(LIBM_PATH m)
   FIND_LIBRARY(LIBRT_PATH rt)
   FIND_LIBRARY(LIBDL_PATH dl)
   FIND_LIBRARY(LIBPTHREAD_PATH pthread)
   
   IF ("${LIBM_PATH}" STREQUAL "LIBM_PATH-NOTFOUND")
      MESSAGE (SEND_ERROR "#### libm.a is missing ####")
   ENDIF ()
   IF ("${LIBRT_PATH}" STREQUAL "LIBRT_PATH-NOTFOUND")
      MESSAGE (SEND_ERROR "#### librt.a is missing ####")
   ENDIF ()
   IF ("${LIBDL_PATH}" STREQUAL "LIBDL_PATH-NOTFOUND")
      MESSAGE (SEND_ERROR "#### libdl.a is missing ####")
   ENDIF ()
   IF ("${LIBPTHREAD_PATH}" STREQUAL "LIBPTHREAD_PATH-NOTFOUND")
      MESSAGE (SEND_ERROR "#### libpthread.a is missing ####")
   ENDIF ()
   
   # Restore the CMake variable
   SET(CMAKE_FIND_LIBRARY_SUFFIXES ${CPY_CMAKE_FIND_LIBRARY_SUFFIXES})
ENDIF ()


## ------------------------------------------------------------------##
##                       Run the compilation                         ##
## ------------------------------------------------------------------##
# ---- Display supported architectures
FOREACH(ARCH ${ARCHS})
   MESSAGE ("-- Architecture ${ARCH} is handled")
ENDFOREACH(ARCH)

# ---- Set variables used in the project
SET(LIBRARY_OUTPUT_PATH    "${CMAKE_SOURCE_DIR}/lib")
SET(EXECUTABLE_OUTPUT_PATH "${CMAKE_SOURCE_DIR}/bin")
SET(INCLUDE_OUTPUT_PATH    "${CMAKE_SOURCE_DIR}/include")
SET(MAQAO_ROOT_PATH        "${CMAKE_SOURCE_DIR}")
SET(GRAPHS_PATH            "/tmp/")
SET(INSTRU_PATH            "/tmp/")
SET(_LUA_BIN ${LUA_BIN})
SET(_INSTALL_DIR ${CMAKE_SOURCE_DIR})
FILE(MAKE_DIRECTORY ${GRAPHS_PATH} ${INSTRU_PATH})

# ---- Specify where are includes
INCLUDE_DIRECTORIES(${INCLUDE_OUTPUT_PATH} ${LUA_INCLUDE_OUTPUT_PATH}) 


# ---- Specify paths to sub-directories
ADD_SUBDIRECTORY(${CMAKE_SOURCE_DIR}/src)

 
# ---- Select man files to install
INSTALL( FILES "${CMAKE_CURRENT_SOURCE_DIR}/doc/man/maqao.1"                  DESTINATION man/man1)
INSTALL( FILES "${CMAKE_CURRENT_SOURCE_DIR}/doc/man/maqao-analyze.1"          DESTINATION man/man1)
INSTALL( FILES "${CMAKE_CURRENT_SOURCE_DIR}/doc/man/maqao-cqa.1"              DESTINATION man/man1)
INSTALL( FILES "${CMAKE_CURRENT_SOURCE_DIR}/doc/man/maqao-lprof.1"            DESTINATION man/man1)
#INSTALL( FILES "${CMAKE_CURRENT_SOURCE_DIR}/doc/man/maqao-instrument.1"       DESTINATION man/man1)
INSTALL( FILES "${CMAKE_CURRENT_SOURCE_DIR}/doc/man/maqao-madras.1"           DESTINATION man/man1)
INSTALL( FILES "${CMAKE_CURRENT_SOURCE_DIR}/doc/man/maqao-mil.1"              DESTINATION man/man1)
INSTALL( FILES "${CMAKE_CURRENT_SOURCE_DIR}/doc/man/maqao-oneview.1"          DESTINATION man/man1)
#INSTALL( FILES "${CMAKE_CURRENT_SOURCE_DIR}/doc/man/maqao-memory.1"           DESTINATION man/man1)

# ---- Add files to remove with distclean command
SET(server_path "${CMAKE_CURRENT_SOURCE_DIR}/src/plugins/built_in")
ADD_CUSTOM_TARGET(distclean DEPENDS 
                                    # MAQAO core
                                    distclean_maqao distclean_libtroll distclean_libdwarf
                                    # MAQAO Lua API 
                                    ${DISTCLEAN_LUA} distclean_builtin distclean_abstractobjects distclean_crosscompil
                                    # MAQAO doc
                                    distclean_doc
         COMMAND ${CMAKE_COMMAND} -E remove_directory ${LIBRARY_OUTPUT_PATH} 
         COMMAND ${CMAKE_COMMAND} -E remove_directory ${EXECUTABLE_OUTPUT_PATH} 
         COMMAND ${CMAKE_COMMAND} -E remove_directory ${INCLUDE_OUTPUT_PATH} 
         VERBATIM
         )

# ---- Generate documentation
ADD_SUBDIRECTORY(${CMAKE_SOURCE_DIR}/doc)

