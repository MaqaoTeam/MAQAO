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

### ---- Generates the build information (date & git commit id) ---- ####
SET(BUILD_INFO "${VERSION_HASH}")
# Retrieving hash of the last GIT commit
EXECUTE_PROCESS(COMMAND git log -1 --pretty="%H"
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                OUTPUT_VARIABLE GIT_COMMIT_HASH
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_VARIABLE NO_GIT_COMMIT)
# Checking that we actually are in a git repository
IF ("x${NO_GIT_COMMIT}" STREQUAL "x" AND "x${BUILD_INFO}" STREQUAL "x${VERSION_HASH_UNKNOWN}" AND NOT "x${GIT_COMMIT_HASH}" STREQUAL "x")
    # Removing quotes around values (added by cmake for some reason)
    STRING(REPLACE "\"" "" GIT_COMMIT_HASH ${GIT_COMMIT_HASH})
    # Retrieving date of the last GIT commit
    EXECUTE_PROCESS(COMMAND git log -1 --pretty="%ai"
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    OUTPUT_VARIABLE GIT_COMMIT_DATE
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_VARIABLE NO_GIT_DATE)
    IF("x${NO_GIT_DATE}" STREQUAL "x" AND NOT "x${GIT_COMMIT_DATE}" STREQUAL "x")
        # Removing quotes around values (added by cmake for some reason)
        STRING(REPLACE "\"" "" GIT_COMMIT_DATE ${GIT_COMMIT_DATE})

        # Building the string containing the date only (used in man printing)
        STRING(REGEX REPLACE "([^\\-]*)-([^\\-]*)-([^\\ ]*) .*" "\\1/\\2/\\3" MAQAO_DATE ${GIT_COMMIT_DATE})

        # Building the string containing the full date (used in build info)
        STRING(REGEX REPLACE " \\+[0-9]*" "" GIT_COMMIT_DATE ${GIT_COMMIT_DATE})  # Removing trailing timezone indications
        STRING(REPLACE ":" "" GIT_COMMIT_DATE ${GIT_COMMIT_DATE})                 # Removing colon separators in hour
        STRING(REPLACE "-" "" GIT_COMMIT_DATE ${GIT_COMMIT_DATE})                 # Removing dash separator in date
        STRING(REPLACE " " "-" GIT_COMMIT_DATE ${GIT_COMMIT_DATE})                # Replacing space with dash between date and hour
    ENDIF("x${NO_GIT_DATE}" STREQUAL "x" AND NOT "x${GIT_COMMIT_DATE}" STREQUAL "x")

    SET(BUILD_INFO "${GIT_COMMIT_HASH}::${GIT_COMMIT_DATE}")
ENDIF("x${NO_GIT_COMMIT}" STREQUAL "x" AND "x${BUILD_INFO}" STREQUAL "x${VERSION_HASH_UNKNOWN}" AND NOT "x${GIT_COMMIT_HASH}" STREQUAL "x")

# If no date is available from git, using the compilation date
IF("x${MAQAO_DATE}" STREQUAL "x")
    STRING(TIMESTAMP MAQAO_DATE "%Y/%m/%d")
ENDIF("x${MAQAO_DATE}" STREQUAL "x")

# ---- Configure the config.h file
SET(PREFIX "${CMAKE_SOURCE_DIR}")
CONFIGURE_FILE(config.h.in ${INCLUDE_OUTPUT_PATH}/config.h @ONLY)
CONFIGURE_FILE(version.h.in ${INCLUDE_OUTPUT_PATH}/version.h @ONLY)

FILE(COPY arch.h             DESTINATION ${INCLUDE_OUTPUT_PATH})
FILE(COPY maqaoerrs.h        DESTINATION ${INCLUDE_OUTPUT_PATH})

FILE(GLOB 
   errors_files
   maqaoerrs.c
)

# ---- Specify paths to sub-directories
UNSET(LIBS_SUPP CACHE)
ADD_SUBDIRECTORY(${LUA_DIR})
ADD_SUBDIRECTORY(common)
ADD_SUBDIRECTORY(asm)
ADD_SUBDIRECTORY(madras)
ADD_SUBDIRECTORY(analyze)
ADD_SUBDIRECTORY(plugins)
ADD_SUBDIRECTORY(maqao)

# ---- Compile maqao
### --- List arch-specific files --- ###

SET(arch_objs-static "")
foreach(ARCH ${ARCHS})
   SET(arch_objs-static "${arch_objs-static};$<TARGET_OBJECTS:mdsmbl${ARCH}-obj-static>")
   SET(arch_objs-static "${arch_objs-static};$<TARGET_OBJECTS:mpatch${ARCH}-obj-static>")
   SET(arch_objs-static "${arch_objs-static};$<TARGET_OBJECTS:masmbl${ARCH}-obj-static>")
   SET(arch_objs-static "${arch_objs-static};$<TARGET_OBJECTS:march${ARCH}-obj-static>")
   SET(arch_objs-static "${arch_objs-static};$<TARGET_OBJECTS:mext${ARCH}-obj-static>")
   SET(arch_objs-static "${arch_objs-static};$<TARGET_OBJECTS:exts${ARCH}-obj-static>")
   SET(arch_objs-static "${arch_objs-static};$<TARGET_OBJECTS:uarchdetect${ARCH}-obj-static>")
endforeach(ARCH)

SET(arch_objs-dyn "")
IF (is_WINDOWS)
   foreach(ARCH ${ARCHS})
      SET(arch_objs-dyn "${arch_objs-dyn};$<TARGET_OBJECTS:mdsmbl${ARCH}-obj-dyn>")
      SET(arch_objs-dyn "${arch_objs-dyn};$<TARGET_OBJECTS:mpatch${ARCH}-obj-dyn>")
      SET(arch_objs-dyn "${arch_objs-dyn};$<TARGET_OBJECTS:masmbl${ARCH}-obj-dyn>")
      SET(arch_objs-dyn "${arch_objs-dyn};$<TARGET_OBJECTS:march${ARCH}-obj-dyn>")
      SET(arch_objs-dyn "${arch_objs-dyn};$<TARGET_OBJECTS:mext${ARCH}-obj-dyn>")
      SET(arch_objs-dyn "${arch_objs-dyn};$<TARGET_OBJECTS:exts${ARCH}-obj-dyn>")
      SET(arch_objs-dyn "${arch_objs-dyn};$<TARGET_OBJECTS:uarchdetect${ARCH}-obj-dyn>")
   endforeach(ARCH)
ELSE ()
   foreach(ARCH ${ARCHS})
      SET(arch_objs-dyn "${arch_objs-dyn};$<TARGET_OBJECTS:mdsmbl${ARCH}-obj-static>")
      SET(arch_objs-dyn "${arch_objs-dyn};$<TARGET_OBJECTS:mpatch${ARCH}-obj-static>")
      SET(arch_objs-dyn "${arch_objs-dyn};$<TARGET_OBJECTS:masmbl${ARCH}-obj-static>")
      SET(arch_objs-dyn "${arch_objs-dyn};$<TARGET_OBJECTS:march${ARCH}-obj-static>")
      SET(arch_objs-dyn "${arch_objs-dyn};$<TARGET_OBJECTS:mext${ARCH}-obj-static>")
      SET(arch_objs-dyn "${arch_objs-dyn};$<TARGET_OBJECTS:exts${ARCH}-obj-static>")
      SET(arch_objs-dyn "${arch_objs-dyn};$<TARGET_OBJECTS:uarchdetect${ARCH}-obj-static>")
   endforeach(ARCH)
ENDIF ()

IF ("${MAQAO_LINKING}" STREQUAL "EXTERNAL_DYNAMIC")
   # Do not add the -static flag
   MESSAGE ("-- MAQAO binary will be dynamically linked")
   
ELSE () #IF ("${MAQAO_LINKING}" STREQUAL "STATIC")
   # Add the -static flag if not on MacOSX (because this flag is not supported on this OS)
   MESSAGE ("-- MAQAO binary will be statically linked")
   IF (NOT APPLE)
      SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
   ENDIF (NOT APPLE)
ENDIF ()


### --- List LUA plugins stubs --- ###

SET(all_objs_stub-static  "")
SET(all_objs_stub-dyn     "")
SET(mtlrt_objs-static "")
SET(mtlrt_objs-dyn "")



### --- Handling of modules --- ###

FOREACH(file ${plugins_sources})

   # Get the last part of "file"
   GET_FILENAME_COMPONENT(file_name ${file} NAME)

   # Check that the module should be compiled and that it has a c-lua stub
   IF(${file_name}_enabled
   AND EXISTS "${file}/lib/stub")

      # Add stub objects
      SET(all_objs_stub-static      "${all_objs_stub-static};$<TARGET_OBJECTS:${file_name}_c_lua-obj-static>")
      SET(all_objs_stub-dyn         "${all_objs_stub-dyn};$<TARGET_OBJECTS:${file_name}_c_lua-obj-dyn>")
      IF ("${file_name}" STREQUAL "mtl")
         SET(mtlrt-objs-static      $<TARGET_OBJECTS:mtlrt-obj-static>)
         SET(mtlrt-objs-dyn         $<TARGET_OBJECTS:mtlrt-obj-dyn>)
      ENDIF ()
   ENDIF ()
   
ENDFOREACH(file)



### --- List sources files --- ###
# List static sources #
SET(maqao_sources-static      $<TARGET_OBJECTS:mmaqao-obj-static>
                              $<TARGET_OBJECTS:mcommon-obj-static>
                              $<TARGET_OBJECTS:masm-obj-static>
                              $<TARGET_OBJECTS:mdisass-obj-static>
                              $<TARGET_OBJECTS:manalyze-obj-static>
                              $<TARGET_OBJECTS:madras-obj-static>
                              $<TARGET_OBJECTS:mtroll-obj-static>
                              $<TARGET_OBJECTS:mpatch-obj-static>
                              $<TARGET_OBJECTS:mworm-obj-static>
                              $<TARGET_OBJECTS:stone-obj-static>
                              $<TARGET_OBJECTS:masmbl-obj-static>
                              ${arch_objs-static}
                              $<TARGET_OBJECTS:dwarf-obj-static>
                              $<TARGET_OBJECTS:elf-obj-static>
                              $<TARGET_OBJECTS:dwarfreader-obj-static>
                              $<TARGET_OBJECTS:mdbg-obj-static>
                              ${CMAKE_CURRENT_SOURCE_DIR}/maqao/linit.c
                              $<TARGET_OBJECTS:fs_lua-obj-static>
                              $<TARGET_OBJECTS:lfs_lua-obj-static>               
                              $<TARGET_OBJECTS:madras_lua-obj-static>
                              $<TARGET_OBJECTS:bitops_lua-obj-static>
                              $<TARGET_OBJECTS:common_c_lua-obj-static>
                              $<TARGET_OBJECTS:errcode_c_lua-obj-static>
                              $<TARGET_OBJECTS:abstract_objects_c_lua-obj-static>
                              $<TARGET_OBJECTS:classes_c_lua-obj-static>
                              ${all_objs_stub-static}
                              ${mtlrt-objs-static}
)
IF (NOT is_WINDOWS)
   SET(maqao_sources-static   ${maqao_sources-static} 
                              $<TARGET_OBJECTS:fcgi-obj-static>
                              $<TARGET_OBJECTS:fcgx_lua-obj-static>
                              $<TARGET_OBJECTS:madras-module-static>)
ENDIF (NOT is_WINDOWS)

# List dynamic sources #
SET(maqao_sources-dyn         $<TARGET_OBJECTS:mmaqao-obj-dyn>
                              $<TARGET_OBJECTS:mcommon-obj-dyn>
                              $<TARGET_OBJECTS:masm-obj-dyn>
                              $<TARGET_OBJECTS:mdisass-obj-dyn>
                              $<TARGET_OBJECTS:manalyze-obj-dyn>
                              $<TARGET_OBJECTS:madras-obj-dyn>
                              $<TARGET_OBJECTS:mtroll-obj-dyn>
                              $<TARGET_OBJECTS:mpatch-obj-dyn>
                              $<TARGET_OBJECTS:stone-obj-dyn>
                              $<TARGET_OBJECTS:mworm-obj-dyn>
                              $<TARGET_OBJECTS:masmbl-obj-dyn>
                              ${arch_objs-dyn}
                              $<TARGET_OBJECTS:dwarf-obj-dyn>
                              $<TARGET_OBJECTS:elf-obj-dyn>
                              $<TARGET_OBJECTS:dwarfreader-obj-dyn>
                              $<TARGET_OBJECTS:mdbg-obj-dyn>
                              ${error_files}
                              $<TARGET_OBJECTS:madras_lua-obj-dyn>
                              $<TARGET_OBJECTS:abstract_objects_c_lua-obj-dyn>
                              $<TARGET_OBJECTS:classes_c_lua-obj-dyn>
)
IF (NOT is_WINDOWS)
   SET(maqao_sources-dyn      ${maqao_sources-dyn} 
                              $<TARGET_OBJECTS:fcgi-obj-dyn>
                              $<TARGET_OBJECTS:fcgx_lua-obj-dyn>
                              $<TARGET_OBJECTS:madras-module-dyn>)
ENDIF (NOT is_WINDOWS)



### --- Create the MAQAO binary --- ###
ADD_EXECUTABLE(maqao ${CMAKE_CURRENT_SOURCE_DIR}/maqao/maqao.c
                  ${maqao_sources-static}
)
ADD_DEPENDENCIES(maqao do_dwarf ${DO_LUA_DEPENDENCY} do_luastatic)
TARGET_INCLUDE_DIRECTORIES(maqao PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/maqao)
SET_TARGET_PROPERTIES(maqao PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}")

# Link the MAQAO binary to the required libraries #
TARGET_LINK_LIBRARIES(maqao ${LUA_LIB_STATIC})
IF (NOT is_WINDOWS)
   TARGET_LINK_LIBRARIES(maqao m dl pthread -Wl,--allow-multiple-definition ${LIBS_SUPP}) # allow-multiple-definition: libgcc already defines some libunwind symbols...
ENDIF ()
IF (IS_STDCXX)
   TARGET_LINK_LIBRARIES(maqao ${STDCXX})                    
ENDIF (IS_STDCXX)
                   
# If needed, strip the binary #
IF (${STRIP})
   IF (is_UNIX)
      ADD_CUSTOM_COMMAND(TARGET maqao POST_BUILD COMMAND ${CMAKE_STRIP} ${EXECUTABLE_OUTPUT_PATH}/maqao)
   ENDIF ()
ENDIF (${STRIP})   

# Installs the MAQAO binary #
INSTALL( TARGETS maqao
         DESTINATION bin
         PERMISSIONS OWNER_READ OWNER_EXECUTE
                     GROUP_READ GROUP_EXECUTE
                     WORLD_READ WORLD_EXECUTE)


### --- Generate the static MAQAO library --- ###
ADD_LIBRARY (maqao-lib-static STATIC
            ${maqao_sources-static}
            )
ADD_DEPENDENCIES(maqao-lib-static maqao ${DO_LUA_DEPENDENCY} do_luastatic)
TARGET_INCLUDE_DIRECTORIES(maqao-lib-static PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/maqao)
SET_TARGET_PROPERTIES(maqao-lib-static PROPERTIES OUTPUT_NAME maqao)
SET_TARGET_PROPERTIES(maqao-lib-static PROPERTIES COMPILE_FLAGS "${C_STATIC_FLAGS}")

# Link the static MAQAO library to the required libraries #
TARGET_LINK_LIBRARIES(maqao-lib-static ${LUA_LIB_STATIC})
IF (IS_STDCXX)
   TARGET_LINK_LIBRARIES(maqao-lib-static ${STDCXX})
ENDIF (IS_STDCXX)

### --- Generate the dynamic MAQAO library --- ###
ADD_LIBRARY (maqao-lib-dyn SHARED
            ${maqao_sources-dyn}
            )
ADD_DEPENDENCIES(maqao-lib-dyn do_dwarf ${DO_LUA_DEPENDENCY})
IF (is_WINDOWS)
   SET_TARGET_PROPERTIES(maqao-lib-dyn PROPERTIES OUTPUT_NAME maqao_d)
ELSEIF (is_UNIX)
   SET_TARGET_PROPERTIES(maqao-lib-dyn PROPERTIES OUTPUT_NAME maqao)
ENDIF ()
SET_TARGET_PROPERTIES(maqao-lib-dyn PROPERTIES COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")

# Link the dynamic MAQAO library to the required libraries #
IF (is_WINDOWS)
   TARGET_LINK_LIBRARIES(maqao-lib-dyn ${LUA_LIB_DYN})
ELSEIF (is_UNIX)
   TARGET_LINK_LIBRARIES(maqao-lib-dyn ${LUA_LIB_DYN} m dl pthread ${LIBS_SUPP})
ENDIF ()
IF (IS_STDCXX)
   TARGET_LINK_LIBRARIES(maqao-lib-dyn ${STDCXX})
ENDIF (IS_STDCXX)



### --- Install the headers --- ###
FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/maqao/libextends.h DESTINATION ${INCLUDE_OUTPUT_PATH}) 
FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/maqao/libmmaqao.h DESTINATION ${INCLUDE_OUTPUT_PATH}/)
                
### --- For cleaning files generated by cmake --- ###
ADD_CUSTOM_TARGET(distclean_maqao_LuaConsts
        COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_SOURCE_DIR}/src/plugins/built_in/classes/Consts_c.lua
        COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_SOURCE_DIR}/src/plugins/built_in/classes/Consts_errs.lua
)
FOREACH(ARCH ${ARCHS})
    ADD_CUSTOM_TARGET(distclean_maqao_LuaConsts_${ARCH} 
        COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_SOURCE_DIR}/src/plugins/built_in/classes/Consts/${ARCH}_c.lua
    )
    add_dependencies(distclean_maqao_LuaConsts distclean_maqao_LuaConsts_${ARCH})
ENDFOREACH(ARCH)