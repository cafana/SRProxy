cmake_minimum_required(VERSION 3.14)

# Bump this by hand when publishing a new release.
project(SRProxy VERSION 00.45 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# The headers require C++17 (inline variables, if constexpr, *_v traits).
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Dependencies.  SRProxy compiles nothing itself, but its headers pull in ROOT
# and consumers build the shipped BasicTypesProxy.cxx against ROOT, so the
# exported target carries ROOT as an interface dependency.
# ---------------------------------------------------------------------------
find_package(ROOT REQUIRED CONFIG)

# ---------------------------------------------------------------------------
# The files that make up the installed package:
#   * headers + BasicTypesProxy.cxx  -> <prefix>/include/SRProxy  (source-shipped)
#   * gen_srproxy generator script   -> <prefix>/bin
# Exceptions.h from the old UPS file list is dropped: it no longer exists.
# ---------------------------------------------------------------------------
set(SRPROXY_INSTALLED_SOURCES
    BasicTypesProxy.h
    BasicTypesProxy.cxx
    FlatBasicTypes.h
    IBranchPolicy.h)

# Stage the shipped files under an SRProxy/ subdirectory in the build tree so
# the "SRProxy/..." include paths used inside the sources resolve for build-tree
# (add_subdirectory) consumers, mirroring the install layout.
set(SRPROXY_STAGED_INCLUDE ${CMAKE_CURRENT_BINARY_DIR}/include)
foreach(_f ${SRPROXY_INSTALLED_SOURCES})
  configure_file(${_f} ${SRPROXY_STAGED_INCLUDE}/SRProxy/${_f} COPYONLY)
endforeach()

# ---------------------------------------------------------------------------
# Header/source-only interface target.  No library is compiled (UPS-faithful):
# it only propagates the include directory, the C++17 requirement, and ROOT.
# ---------------------------------------------------------------------------
add_library(SRProxy INTERFACE)
add_library(SRProxy::SRProxy ALIAS SRProxy)

target_compile_features(SRProxy INTERFACE cxx_std_17)

target_include_directories(SRProxy INTERFACE
    $<BUILD_INTERFACE:${SRPROXY_STAGED_INCLUDE}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# ROOT libraries the sources reference: TError (Core), TFile (RIO),
# TTree/TLeaf/TBranch (Tree), TFormLeafInfo/TTreeFormula (TreePlayer).
target_link_libraries(SRProxy INTERFACE
    ROOT::Core ROOT::RIO ROOT::Tree ROOT::TreePlayer)

# ---------------------------------------------------------------------------
# Install rules.
# ---------------------------------------------------------------------------
install(FILES ${SRPROXY_INSTALLED_SOURCES}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/SRProxy)

install(PROGRAMS gen_srproxy
        DESTINATION ${CMAKE_INSTALL_BINDIR})

install(TARGETS SRProxy EXPORT SRProxyTargets)

# ---------------------------------------------------------------------------
# find_package(SRProxy) support: exported target + config + version file.
# ---------------------------------------------------------------------------
set(SRPROXY_CMAKE_CONFIG_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/SRProxy)

install(EXPORT SRProxyTargets
        FILE SRProxyTargets.cmake
        NAMESPACE SRProxy::
        DESTINATION ${SRPROXY_CMAKE_CONFIG_DIR})

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/SRProxyConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/SRProxyConfig.cmake
    INSTALL_DESTINATION ${SRPROXY_CMAKE_CONFIG_DIR})

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/SRProxyConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/SRProxyConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/SRProxyConfigVersion.cmake
    DESTINATION ${SRPROXY_CMAKE_CONFIG_DIR})

# ---------------------------------------------------------------------------
# IDE convenience target.
# The INTERFACE library above produces no build artifact,
# so IDEs have nothing to "build" and can't attach a profile to it.
# This target gives them one: building it installs the package,
# and the SOURCES list keeps the shipped files visible/indexed in the IDE.
# ---------------------------------------------------------------------------
add_custom_target(install-srproxy
    COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR}
    SOURCES ${SRPROXY_INSTALLED_SOURCES} gen_srproxy
    COMMENT "Installing SRProxy into ${CMAKE_INSTALL_PREFIX}")
