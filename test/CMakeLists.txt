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
  )

add_library(test_proxy SHARED TestTargetproxy.cxx)
target_include_directories(test_proxy PUBLIC 
  ${CMAKE_CURRENT_SOURCE_DIR} 
  ${CMAKE_SOURCE_DIR}/src/include)
target_link_libraries(test_proxy PUBLIC SRProxy_BasicTypes)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/TestTargetproxy.h 
              ${CMAKE_CURRENT_BINARY_DIR}/FwdDeclare.h 
        DESTINATION include/testproxy)
install(TARGETS test_proxy DESTINATION test/lib)