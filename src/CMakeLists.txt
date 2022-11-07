add_library(SRProxy_BasicTypes SHARED BasicTypesProxy.cxx)
target_include_directories(SRProxy_BasicTypes PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(SRProxy_BasicTypes ROOT::ROOT)
install(TARGETS SRProxy_BasicTypes DESTINATION lib)