add_library(SRProxy_BasicTypes SHARED BasicTypesProxy.cxx)

target_include_directories(SRProxy_BasicTypes PUBLIC 
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> 
  $<INSTALL_INTERFACE:include> )
set_target_properties(SRProxy_BasicTypes PROPERTIES EXPORT_NAME SRProxy::BasicTypes)
target_link_libraries(SRProxy_BasicTypes PUBLIC ROOT::TreePlayer ROOT::Tree ROOT::Core)

add_library(SRProxy::BasicTypes ALIAS SRProxy_BasicTypes)

add_library(SRProxy_FlatTypes INTERFACE)
target_include_directories(SRProxy_FlatTypes INTERFACE 
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> 
  $<INSTALL_INTERFACE:include> )
set_target_properties(SRProxy_FlatTypes PROPERTIES EXPORT_NAME SRProxy::FlatTypes)
target_link_libraries(SRProxy_FlatTypes INTERFACE ROOT::TreePlayer ROOT::Tree ROOT::Core)

add_library(SRProxy::FlatTypes ALIAS SRProxy_FlatTypes)

install(TARGETS SRProxy_BasicTypes SRProxy_FlatTypes EXPORT SRProxyTargets DESTINATION lib)
install(DIRECTORY include/SRProxy DESTINATION include)