add_executable(gen_srproxy gen_srproxy.cxx)
target_link_libraries(gen_srproxy ROOT::ROOT fmt::fmt)
target_compile_definitions(gen_srproxy PUBLIC 
  SRProxy_VERSION="${SRProxy_VERSION}"
  BUILD_ROOT_VERSION="${ROOT_VERSION}"
  BUILD_ROOT_LIBRARY_DIR="${ROOT_LIBRARY_DIR}")
install(TARGETS gen_srproxy DESTINATION bin)