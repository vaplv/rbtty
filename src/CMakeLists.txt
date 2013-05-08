cmake_minimum_required(VERSION 2.6)
project(rbtty C)
enable_testing()

################################################################################
# Setup compiler
################################################################################
if(NOT CMAKE_COMPILER_IS_GNUCC)
  message(FATAL_ERROR "Unsupported compiler")
endif(NOT CMAKE_COMPILER_IS_GNUCC)

set(CMAKE_DEBUG_POSTFIX "-dbg")

set(CMAKE_C_FLAGS "-pedantic -std=c99 -Wall -Wextra -Wcast-align -Wmissing-declarations -Wmissing-prototypes -fvisibility=hidden -fstrict-aliasing -Wl,-z,defs -Wconversion")
set(CMAKE_C_FLAGS_DEBUG "-g")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")

################################################################################
# Check dependencies
################################################################################
macro(check_dependency lib header)
  find_path(${lib}_INCLUDE_DIR ${header})
  find_library(${lib}_LIBRARY ${lib})

  if(NOT ${lib}_INCLUDE_DIR OR NOT ${lib}_LIBRARY)
    message(FATAL_ERROR "Can't find ${lib} header/library")
  else()
    message(STATUS "${lib} found: ${${lib}_LIBRARY}")
  endif()
  include_directories(${${lib}_INCLUDE_DIR})
endmacro()

check_dependency(font-rsrc font_rsrc.h)
check_dependency(lp lp/lp.h)
check_dependency(rbi rb/rbi.h)
check_dependency(sl sl/sl.h)
check_dependency(snlsys snlsys/snlsys.h)

check_dependency(font-rsrc-dbg font_rsrc.h)
check_dependency(lp-dbg lp/lp.h)
check_dependency(sl-dbg sl/sl.h)
check_dependency(snlsys-dbg snlsys/snlsys.h)

################################################################################
# Target
################################################################################
set(RBTTY_FILES_INC rbtty_error.h rbtty_screen.h rbtty.h)
set(RBTTY_FILES_SRC rbtty_screen.c rbtty.c)

add_library(rbtty SHARED ${RBTTY_FILES_SRC} ${RBTTY_FILES_INC})

target_link_libraries(rbtty debug 
  ${font-rsrc-dbg_LIBRARY} 
  ${lp-dbg_LIBRARY} 
  ${sl-dbg_LIBRARY} 
  ${snlsys-dbg_LIBRARY})

target_link_libraries(rbtty optimized
  ${font-rsrc_LIBRARY} 
  ${lp_LIBRARY} 
  ${sl_LIBRARY}
  ${snlsys_LIBRARY})
  
################################################################################
# Output files
################################################################################
install(FILES rbtty.h DESTINATION include)
install(TARGETS rbtty LIBRARY DESTINATION lib)
