cmake_minimum_required (VERSION 3.14 FATAL_ERROR)

project(SRProxy VERSION 0.46.0 LANGUAGES CXX)

set (SRProxy_VERSION ${PROJECT_VERSION})

SET(SRProxy_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD ${SRProxy_CXX_STANDARD})

#Use the compilers found in the path
find_program(CMAKE_C_COMPILER NAMES $ENV{CC} gcc PATHS ENV PATH NO_DEFAULT_PATH)
find_program(CMAKE_CXX_COMPILER NAMES $ENV{CXX} g++ PATHS ENV PATH NO_DEFAULT_PATH)

option(SRProxy_TESTS_ENABLED "Whether to enable tests, OFF by default" OFF)
option(SRProxy_TEST_PYTHON_BINDINGS "Whether to enable test of python bindings, Off by default" OFF)

LIST(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake/Modules)

include(CPM)

CPMAddPackage(
    NAME fmt
    GIT_TAG 10.2.1
    GITHUB_REPOSITORY fmtlib/fmt
)

find_package(ROOT 6.20 REQUIRED)

include(GenSRProxy)

add_subdirectory(app)
add_subdirectory(src)

if(SRProxy_TESTS_ENABLED)
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
        NAMESPACE SRProxy::
        DESTINATION lib/cmake/SRProxy)
install(FILES "${PROJECT_BINARY_DIR}/SRProxyConfigVersion.cmake"
              "${PROJECT_BINARY_DIR}/SRProxyConfig.cmake"
              "${PROJECT_SOURCE_DIR}/cmake/Modules/GenSRProxy.cmake"
        DESTINATION lib/cmake/SRProxy)

configure_file(${CMAKE_CURRENT_LIST_DIR}/cmake/Templates/setup.SRProxy.sh.in
  ${PROJECT_BINARY_DIR}/setup.SRProxy.sh @ONLY)
install(PROGRAMS ${PROJECT_BINARY_DIR}/setup.SRProxy.sh DESTINATION bin)

