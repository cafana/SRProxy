GenSRProxy(
  HEADER test.h
  OUTPUT_NAME TestTargetproxy
  TARGETNAME test::TestTarget
  INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
  PROLOG prolog.h
  EPILOG_FWD epilog_fwd.h
  EPILOG epilog.h
  EXTRAS
    TestConstituentA extraA.h
    TestConstituentC extraC.h
  DEFINITIONS
    NEEDTHISTOBUILD=int
    REVEAL_TestTarget
  EMIT_PYTHON_BINDINGS
  VVERBOSE
  )

GenSRProxy(
  HEADER test.h
  FLAT
  OUTPUT_NAME TestTargetflat
  TARGETNAME test::TestTarget
  INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
  PROLOG prolog.h
  EPILOG_FWD epilog_fwd.h
  EPILOG epilog.h
  EXTRAS
    TestConstituentA extraA.h
    TestConstituentC extraC.h
  DEFINITIONS
    NEEDTHISTOBUILD=int
    REVEAL_TestTarget
  VVERBOSE
  )

add_library(test_proxy SHARED TestTargetproxy.cxx TestTargetflat.cxx)
target_compile_definitions(test_proxy PUBLIC NEEDTHISTOBUILD=int REVEAL_TestTarget)
target_include_directories(test_proxy PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/src/include)
target_link_libraries(test_proxy PUBLIC SRProxy_BasicTypes)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/TestTargetproxy.h
              ${CMAKE_CURRENT_BINARY_DIR}/FwdDeclare.h
        DESTINATION include/testproxy)
install(TARGETS test_proxy DESTINATION test/lib)


if(SRProxy_TEST_PYTHON_BINDINGS)

  find_package(Python3 REQUIRED COMPONENTS Development Interpreter)

  # PYTHON PATHS
  set(SRProxy_PYTHONPATH "python/${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}")

  include(FetchContent)

  FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG        f5fbe86 # release-3.0.1
  )

  FetchContent_MakeAvailable(pybind11)

  pybind11_add_module(pytestproxy SHARED TestTargetproxy.pybind.cxx)

  target_include_directories(pytestproxy
    PRIVATE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/include>
      )

  target_link_libraries(pytestproxy
    PRIVATE
      test_proxy)

  install(TARGETS pytestproxy
      LIBRARY DESTINATION ${SRProxy_PYTHONPATH})

endif()
