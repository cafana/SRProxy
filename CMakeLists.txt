cmake_minimum_required (VERSION 3.14 FATAL_ERROR)

project(SRProxy VERSION 0.4.0 LANGUAGES CXX)

set (SRProxy_VERSION ${PROJECT_VERSION})
set (SRProxy_VERSION_NUM 040)

SET(SRPROXY_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD ${SRPROXY_CXX_STANDARD})

#Use the compilers found in the path
find_program(CMAKE_C_COMPILER NAMES $ENV{CC} gcc PATHS ENV PATH NO_DEFAULT_PATH)
find_program(CMAKE_CXX_COMPILER NAMES $ENV{CXX} g++ PATHS ENV PATH NO_DEFAULT_PATH)

option(TESTS_ENABLED "Whether to enable tests, OFF by default" OFF)
option(EMIT_UPS_PRODUCT "Whether to enable building of UPS product, OFF by default" OFF)

if(CMAKE_BUILD_TYPE STREQUAL "")
  set(CMAKE_BUILD_TYPE RELWITHDEBINFO)
elseif(NOT DEFINED CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RELWITHDEBINFO)
endif()

LIST(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake/Modules)

#### RPATH handling #####

set(CMAKE_MACOSX_RPATH 1)

# use, i.e. don't skip the full RPATH for the build tree
SET(CMAKE_SKIP_BUILD_RPATH  FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# the RPATH to be used when installing, but only if it's not a system directory
LIST(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}" isSystemDir)
IF("${isSystemDir}" STREQUAL "-1")
   SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
ENDIF("${isSystemDir}" STREQUAL "-1")


if(EMIT_UPS_PRODUCT)
  add_subdirectory(ups)

  if(NOT "${CMAKE_INSTALL_PREFIX}x" STREQUAL "x")
    set(CMAKE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX}/srproxy/${SRPROXY_UPS_VERSION})
  endif()

endif()

include(CPM)

CPMAddPackage(
    NAME fmt
    GIT_TAG 8.1.1
    GITHUB_REPOSITORY fmtlib/fmt
)

find_package(ROOT REQUIRED)

get_target_property(ROOT_INTERFACE_INCLUDES ROOT::Core INTERFACE_INCLUDE_DIRECTORIES)
#Older ROOTConfig.cmake do not set up their targets correctly
if("${ROOT_INTERFACE_INCLUDES}" STREQUAL "ROOT_INTERFACE_INCLUDES-NOTFOUND" OR "${ROOT_INTERFACE_INCLUDES}x" STREQUAL "x")
  message(STATUS "Jury Rigging ROOT::Core with additional interface include directories: ${ROOT_INCLUDE_DIRS}")
  target_include_directories(ROOT::Core INTERFACE ${ROOT_INCLUDE_DIRS})
endif()

add_subdirectory(app)
add_subdirectory(src)

if(TESTS_ENABLED)
  add_subdirectory(test)
endif()

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/SRProxyConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)
configure_package_config_file(
  "${CMAKE_CURRENT_LIST_DIR}/cmake/Templates/SRProxyConfig.cmake.in"
  "${PROJECT_BINARY_DIR}/SRProxyConfig.cmake"
  INSTALL_DESTINATION this/is/ignored
  NO_SET_AND_CHECK_MACRO
  NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

install(EXPORT SRProxyTargets
        DESTINATION lib/cmake/SRProxy)
install(FILES "${PROJECT_BINARY_DIR}/SRProxyConfigVersion.cmake"
              "${PROJECT_BINARY_DIR}/SRProxyConfig.cmake"
        DESTINATION lib/cmake/SRProxy)

configure_file(${CMAKE_CURRENT_LIST_DIR}/cmake/Templates/setup.SRProxy.sh.in ${PROJECT_BINARY_DIR}/setup.SRProxy.sh @ONLY)
install(PROGRAMS ${PROJECT_BINARY_DIR}/setup.SRProxy.sh DESTINATION bin)

