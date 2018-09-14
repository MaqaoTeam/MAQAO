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

LINK_DIRECTORIES(${LIBRARY_OUTPUT_PATH})

#####################################################################################################
#### The following code was imported from the CMakeLists.txt at the root of the libdwarf package ####
#####################################################################################################
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(LibdwarfMacros)
# always include project's folder to includes
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)

set(LIBELF_INCLUDE_DIRS ${INCLUDE_OUTPUT_PATH}) #MAQAO: Overriden libelf detection
set(LIBELF_LIBRARIES libelf.so) #MAQAO: Overriden libelf detection

list(APPEND CMAKE_REQUIRED_INCLUDES ${LIBELF_INCLUDE_DIRS})

#MAQAO: Imported from root configure.cmake
include(AutoconfHelper)
#MAQAO: End of imported from root configure.cmake

ac_c_bigendian()
ac_check_headers(sys/types.h sys/stat.h stdlib.h string.h memory.h strings.h inttypes.h stdint.h unistd.h)

##########################################################################################
#### End of code imported from the CMakeLists.txt at the root of the libdwarf package ####
##########################################################################################

# Creating list expected by the code imported from libdwarf CMakeLists.txt 
set(DWARF_TARGETS dwarf-obj-static)
set(DWARF_TYPES OBJECT)
set(DWARF_COMPILE_FLAGS "${C_STATIC_FLAGS}")
list(APPEND DWARF_TARGETS dwarf-obj-dyn)
list(APPEND DWARF_TYPES OBJECT)
list(APPEND DWARF_COMPILE_FLAGS "${C_DYNAMIC_FLAGS}")

# gennames sources have been moved to this directory for cross-compilation
ADD_SUBDIRECTORY (generators)

########################################################################################################################
#### The following code was imported from the CMakeLists.txt in the libdwarf subdirectory from the libdwarf package ####
########################################################################################################################
include(configure.cmake)

set_source_group(SOURCES "Source Files" dwarf_abbrev.c dwarf_alloc.c dwarf_arange.c dwarf_die_deliv.c dwarf_dnames.c dwarf_dsc.c dwarf_elf_access.c dwarf_error.c 
dwarf_form.c dwarf_frame.c dwarf_frame2.c dwarf_frame3.c dwarf_funcs.c dwarf_gdbindex.c dwarf_global.c dwarf_harmless.c dwarf_init_finish.c dwarf_leb.c 
dwarf_line.c dwarf_loc.c dwarf_macro.c dwarf_macro5.c dwarf_original_elf_init.c dwarf_pubtypes.c dwarf_query.c dwarf_ranges.c dwarf_string.c dwarf_tied.c 
dwarf_tsearchhash.c dwarf_types.c dwarf_util.c dwarf_vars.c dwarf_weaks.c dwarf_xu_index.c dwarf_print_lines.c malloc_check.c pro_alloc.c pro_arange.c 
pro_die.c pro_encode_nm.c pro_error.c pro_expr.c pro_finish.c pro_forms.c pro_funcs.c pro_frame.c pro_init.c pro_line.c pro_reloc.c pro_reloc_stream.c 
pro_reloc_symbolic.c pro_pubnames.c pro_section.c pro_types.c pro_vars.c pro_macinfo.c pro_weaks.c)

set_source_group(HEADERS "Header Files" dwarf.h dwarf_abbrev.h dwarf_alloc.h dwarf_arange.h dwarf_base_types.h dwarf_die_deliv.h dwarf_dnames.h dwarf_dsc.h 
dwarf_elf_access.h dwarf_error.h dwarf_frame.h dwarf_funcs.h dwarf_gdbindex.h dwarf_global.h dwarf_harmless.h dwarf_incl.h dwarf_line.h dwarf_loc.h 
dwarf_macro.h dwarf_macro5.h dwarf_opaque.h dwarf_reloc_arm.h dwarf_reloc_mips.h dwarf_reloc_ppc.h dwarf_reloc_ppc64.h dwarf_reloc_x86_64.h dwarf_tsearch.h 
dwarf_types.h dwarf_util.h dwarf_vars.h dwarf_weaks.h dwarf_xu_index.h dwgetopt.h libdwarfdefs.h malloc_check.h pro_alloc.h pro_arange.h pro_die.h 
pro_encode_nm.h pro_error.h pro_expr.h pro_frame.h pro_incl.h pro_line.h pro_macinfo.h pro_opaque.h pro_reloc.h pro_reloc_stream.h pro_reloc_symbolic.h 
pro_section.h pro_types.h pro_util.h)

#set_source_group(GENNAMES_SOURCES "Source Files" gennames.c dwgetopt.c dwarf.h) # MAQAO: gennames was moved to the generators subdirectory 

set_source_group(CONFIGURATION_FILES "Configuration Files" configure.cmake config.h.in.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)
####MAQAO: Added target to keep compatibility with cmake 2.8####
ADD_CUSTOM_TARGET(dwarf_config_files DEPENDS  ${CONFIGURATION_FILES})
####MAQAO: End modification####

#add_executable(gennames ${GENNAMES_SOURCES}) # MAQAO: gennames was moved to the generators subdirectory

#set_folder(gennames libdwarf/generators) # MAQAO: gennames was moved to the generators subdirectory

#msvc_posix(gennames) # MAQAO: gennames was moved to the generators subdirectory

set(GENNAMES_OUTPUT dwarf_names.c dwarf_names.h dwarf_names_enum.h dwarf_names_new.h)

add_custom_command(OUTPUT ${GENNAMES_OUTPUT}
    COMMAND gennames ${dwarf_namestable} -i ${CMAKE_CURRENT_SOURCE_DIR} -o .
    DEPENDS gennames libdwarf.h.in)
	
list(LENGTH DWARF_TARGETS targetCount)
math(EXPR targetCount "${targetCount} - 1")
foreach(i RANGE ${targetCount})
	list(GET DWARF_TARGETS ${i} target)
	list(GET DWARF_TYPES ${i} type)
    list(GET DWARF_COMPILE_FLAGS ${i} flag) # MAQAO: Added compilation flags for compatibility with overall MAQAO compilation
	
####MAQAO: Moved dependency to CONFIGURATION_FILES to the dwarf_config_files target to keep compatibility with cmake 2.8####
	add_library(${target} ${type} ${SOURCES} ${HEADERS} ${GENNAMES_OUTPUT})
    add_dependencies(${target} dwarf_config_files)	
####MAQAO: End modification####
    SET_TARGET_PROPERTIES(${target} PROPERTIES COMPILE_FLAGS ${flag}) # MAQAO: Added compilation flags for compatibility with overall MAQAO compilation

	set_folder(${target} libdwarf)

	target_include_directories(${target} PUBLIC ${LIBELF_INCLUDE_DIRS})

	msvc_posix(${target})

# MAQAO: following code not needed, done elsewhere in MAQAO
#	target_link_libraries(${target} PUBLIC ${LIBELF_LIBRARIES})
	
#	set_target_properties(${target} PROPERTIES OUTPUT_NAME dwarf)

#	set(SUFFIX $<$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>:64>)
#	set(LIBDIR lib${SUFFIX})
#	set(BINDIR bin${SUFFIX})
#
#	install(TARGETS ${target}
#		RUNTIME DESTINATION ${BINDIR}	
#		LIBRARY DESTINATION ${LIBDIR}
#		ARCHIVE DESTINATION ${LIBDIR})

endforeach()

# MAQAO: Following code not needed
#if(UNIX AND shared)
	#target_link_libraries(dwarf-shared PUBLIC z)
#endif()
		
#if(UNIX)
#
#
# The following are very SGI-centric
# psroff is just a troff formatter.
# the .mm files are in ATT/USL/USG mm form.
# Calling it pdfbld or bldpdf is arbitrary, so allow both.
#
#Oriented to using gsroff now.
#set(TROFF /usr/bin/groff)
#set(TROFFDEV "-T ps")
#set(TROFFDEV)
#set(PSTOPDF /usr/bin/ps2pdf)
#set(BUILD_BASE .)
# pr expands tabs to spaces: this avoids problems with tab
# interpretation
#
# The warning about 'cant break line' is a too-long line used
# in the table of contents.
# All pr does here is change each tab to spaces.
# Ignore the warning (and those like it).
#add_custom_command(OUTPUT libdwarf2.1.pdf
#    COMMAND -pr -t -e ${BUILD_BASE}/libdwarf2.1.mm | tbl | ${TROFF} ${TROFFDEV} -mm >libdwarf2.1.ps
#    COMMAND ${PSTOPDF} libdwarf2.1.ps ${CMAKE_CURRENT_BINARY_DIR}/libdwarf2.1.pdf
#    DEPENDS ${BUILD_BASE}/libdwarf2.1.mm)
#    
#add_custom_command(OUTPUT libdwarf2p.1.pdf
#    COMMAND -pr -t -e ${BUILD_BASE}/libdwarf2p.1.mm | tbl | ${TROFF} ${TROFFDEV} -mm >libdwarf2p.1.ps
#    COMMAND ${PSTOPDF} libdwarf2p.1.ps ${CMAKE_CURRENT_BINARY_DIR}/libdwarf2p.1.pdf
#    DEPENDS ${BUILD_BASE}/libdwarf2p.1.mm)
#    
#add_custom_command(OUTPUT mips_extensions.pdf
#    COMMAND -pr ${TROFFDEV} -e ${BUILD_BASE}/mips_extensions.mm | tbl | ${TROFF} ${TROFFDEV} -mm >mips_extensions.ps
#    COMMAND ${PSTOPDF} mips_extensions.ps ${CMAKE_CURRENT_BINARY_DIR}/mips_extensions.pdf
#    DEPENDS ${BUILD_BASE}/mips_extensions.mm)
#    
#add_custom_target(pdfbld
#    DEPENDS libdwarf2.1.pdf libdwarf2p.1.pdf mips_extensions.pdf)
#
#set_folder(pdfbld libdwarf/documentation)
#    
#add_custom_target(bldpdf
#    DEPENDS pdfbld)
#
#set_folder(bldpdf libdwarf/documentation)
#    
#add_custom_target(docbld
#    DEPENDS pdfbld)
#
#set_folder(docbld libdwarf/documentation)
#
#endif()

#############################################################################################################
#### End of code imported from the CMakeLists.txt in the libdwarf subdirectory from the libdwarf package ####
#############################################################################################################

# Add a target to build dependences on dwarf #
ADD_CUSTOM_TARGET(do_dwarf DEPENDS dwarf-obj-static dwarf-obj-dyn)

### --- Install the headers --- ###
FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/dwarf.h    DESTINATION ${INCLUDE_OUTPUT_PATH})
FILE(COPY ${CMAKE_CURRENT_BINARY_DIR}/libdwarf.h DESTINATION ${INCLUDE_OUTPUT_PATH})

### --- For cleaning files generated by cmake --- ###
ADD_CUSTOM_TARGET(distclean_libdwarf
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_SOURCE_DIR}/gennames
    )


