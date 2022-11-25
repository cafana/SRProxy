add_library(SRProxy_BasicTypes SHARED BasicTypesProxy.cxx)

target_include_directories(SRProxy_BasicTypes PUBLIC 
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> 
  $<INSTALL_INTERFACE:include> )
target_link_libraries(SRProxy_BasicTypes ROOT::Core)

add_library(SRProxy::BasicTypes ALIAS SRProxy_BasicTypes)
install(TARGETS SRProxy_BasicTypes EXPORT SRProxyTargets DESTINATION lib)