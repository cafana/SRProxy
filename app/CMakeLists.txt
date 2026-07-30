add_executable(gen_srproxy gen_srproxy.cxx)
target_link_libraries(gen_srproxy ROOT::Cling ROOT::RIO ROOT::Core fmt::fmt)
target_compile_definitions(gen_srproxy PUBLIC 
  SRProxy_VERSION="${PROJECT_VERSION}"
  BUILD_ROOT_VERSION="${ROOT_VERSION}"
  BUILD_ROOT_LIBRARY_DIR="${ROOT_LIBRARY_DIR}")
install(TARGETS gen_srproxy EXPORT SRProxyTargets DESTINATION bin)