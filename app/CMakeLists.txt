add_executable(GenSRProxy GenSRProxy.cxx)
target_link_libraries(GenSRProxy ROOT::ROOT fmt::fmt)
target_compile_definitions(GenSRProxy PUBLIC "SRProxy_VERSION=\"${SRProxy_VERSION}\"")
install(TARGETS GenSRProxy DESTINATION bin)