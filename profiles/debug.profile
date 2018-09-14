# Debug profile for MAQAO compilation for arm64 architecture only
##################################################################

# Sets the list of architectures to be handled by MAQAO, separated by ';'.
# Possible values: arm64. Default value: "arm64"
#SET(ARCHS                    arm64)

# The optimisation level (higher is more optimised).
# Possible values: 0 / 1 / 2 (default) / 3 
SET(OPTIM_LVL                0)

# Set the Lua interpreter.
# Possible values: lua / luajit (default)
#SET(LUA                      luajit)

# Specifies whether the binary must be stripped.
# Possible values: true (default) / false
SET(STRIP                    false)

# Specifies if the debug info and MAQAO debug macros must be activated.
# Possible values: true / false (default)
SET(DEBUG                    true)

# A list of MAQAO Lua modules to not include in the static binary separated by ';'
#SET(EXCLUDE_MODULES          "")

# A list of tags in the source code to not include in the static binary separated by ';'
#SET(EXCLUDE_CODE             "")

# A list of MAQAO Lua modules to include in the static binary separated by ';'
#SET(INCLUDE_MODULES          "")

# Specifies if the MAQAO binary should be linked as a static executable or if the external libraries (libdl, libdm, ...) must be dynamically linked.
# Possible values: STATIC (default) / EXTERNAL_DYNAMIC
#SET(MAQAO_LINKING            STATIC)

# Specifies whether MAQAO is being compiled for a different architecture than the current one. 
# Possible values: true / false (default)
#SET(CROSS_COMPILATION        false)

# Set the compiler used to compile C code for target architecture (local if not cross-compiling).
# Possible values: "gcc", "icc", etc. Default value is system default.
#SET(CMAKE_C_COMPILER         "gcc")

# Set options passed to the C compiler for target architecture (local if not cross-compiling)
# Additional options can be added with names CMAKE_C_COMPILER_ARG2, CMAKE_C_COMPILER_ARG3, etc
#SET(CMAKE_C_COMPILER_ARG1    "")               

# Set the compiler used to compile C code for local architecture in case of cross-compilation
#SET(CC_HOST                  "")

# Set the compiler used to compile C++ code for target architecture (local if not cross-compiling).
# Possible values: "g++", "icpc", etc. Default value is system default.
#SET(CMAKE_CXX_COMPILER       "g++")

# Set options passed to the C++ compiler for target architecture
# Additional options can be added with names CMAKE_CXX_COMPILER_ARG2, CMAKE_CXX_COMPILER_ARG3, etc
#SET(CMAKE_CXX_COMPILER_ARG1  "")

# Set the compiler used to compile C++ code for local architecture in case of cross-compilation
#SET(CXX_HOST                 "")

# Specifies additional directories where to look for libraries. Should only be needed in case of cross-compilation.
#LINK_DIRECTORIES(            "")


