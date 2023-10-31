include(GenSRProxy)

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
  VVERBOSE
  )

add_library(test_proxy SHARED TestTargetproxy.cxx)
target_compile_definitions(test_proxy PUBLIC NEEDTHISTOBUILD=int REVEAL_TestTarget)
target_include_directories(test_proxy PUBLIC 
  ${CMAKE_CURRENT_SOURCE_DIR} 
  ${CMAKE_SOURCE_DIR}/src/include)
target_link_libraries(test_proxy PUBLIC SRProxy_BasicTypes)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/TestTargetproxy.h 
              ${CMAKE_CURRENT_BINARY_DIR}/FwdDeclare.h 
        DESTINATION include/testproxy)
install(TARGETS test_proxy DESTINATION test/lib)

if(PYTHONTESTS_ENABLED)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/pytest)
  #Compare to python implementation like
  add_custom_command(
    OUTPUT pytest/TestTargetproxy.cxx
           pytest/TestTargetproxy.h
           pytest/FwdDeclare.h
    COMMAND python3
    ARGS ${CMAKE_CURRENT_SOURCE_DIR}/gen_srproxy -i test.h -t test::TestTarget
          -o TestTargetproxy
          -p ${CMAKE_CURRENT_SOURCE_DIR} -op ${CMAKE_CURRENT_BINARY_DIR}/pytest
          --prolog ${CMAKE_CURRENT_SOURCE_DIR}/prolog.h 
          --epilog ${CMAKE_CURRENT_SOURCE_DIR}/epilog.h 
          --epilog-fwd ${CMAKE_CURRENT_SOURCE_DIR}/epilog_fwd.h
          --extra TestConstituentA ${CMAKE_CURRENT_SOURCE_DIR}/extraA.h
          --extra TestConsistuentC ${CMAKE_CURRENT_SOURCE_DIR}/extraC.h
          --extra-cflags "-DNEEDTHISTOBUILD=int -DREVEAL_TestTarget"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/pytest )

  add_library(test_pyproxy SHARED pytest/TestTargetproxy.cxx)
  target_compile_definitions(test_pyproxy PUBLIC NEEDTHISTOBUILD=int REVEAL_TestTarget)
  target_include_directories(test_pyproxy PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/pytest
    ${CMAKE_SOURCE_DIR}/src/include)
  target_link_libraries(test_pyproxy PUBLIC SRProxy_BasicTypes)
endif()