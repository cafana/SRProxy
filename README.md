# SRProxy

## Build

Depends on ROOT6, have a copy of ROOT6 set up and try and invoke CMake like:

```
cd /path/to/SRProxy
mkdir build; cd build;
cmake .. -DCMAKE_INSTALL_PREFIX=Linux
make install
```

## gen_srproxy

This programme parses a class description (with the help of Cling) and emits a 'Proxy' class that lets the user read instances of the class from an input TTree in an I/O efficient way where TBranches corresponding to datamembers of that class are only enabled up when they are needed.

Running `gen_srproxy --help` at the time of writing results in:

```
[USAGE] gen_srproxy -i <header_file> -t <classname> -o <filename_stub> [args]

Required arguments:
  -i|--input <header_file>       : The C++ header file that defines the class
  -t|--target <classname>        : The class to generate a proxy for
  -o|--output <filename_stub>    : Output filename stub

Optional arguments:
  -op|--output-path <path>       : A path to prepend to include statements in generated headers
  -od|--output-dir <path>        : The directory to write generated files to
  --prolog <file path>           : A file to include before the generated proxy class defintion
  --epilog <file path>           : A file to include after the generated proxy class definition
  --epilog-fwd <file path>       : A file to include after the list of generated forward declarations
  -I <path>                      : A directory to add to the include path
  -p|--include-path <path1[:p2]> : A PATH-like colon-separate list of directories to add to the include path
  --extra <classname> <file>     : A file to include in the definition of the proxy class for class <classname>
  --flat                         : Generate a 'flat' file reader rather than the objectified proxy class
  --order-alphabetically         : Emit datamembers in alphabetic, rather than declaration, order.

  -v|--verbose                   : Be louder
  -vv|--vverbose                 : Be even louder
  -h|-?|--help                   : Print this message
```

## Using in your project

The recommended way to include this tool in your CMake project is via [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake).

```
  CPMAddPackage(
    NAME SRProxy
    GIT_TAG master
    GITHUB_REPOSITORY luketpickering/SRProxy
  )
  include(${SRProxy_SOURCE_DIR}/cmake/Modules/GenSRProxy.cmake)
```

This will set up the `GenSRProxy` command from [GenSRProxy.cmake](cmake/Modules/GenSRProxy.cmake), which can be used in your project like:

```
GenSRProxy(
  [FLAT] 
  [VERBOSE]
  [HEADER <arg>]
  [OUTPUT_NAME <arg>]
  [OUTPUT_PATH <arg>]
  [TARGETNAME <arg>]
  [PROLOG <arg>]
  [EPILOG <arg>]
  [EPILOG_FWD <arg>]
  [INCLUDE_DIRS <arg1> [<arg2> ...]]
  [DEPENDENCIES <arg1> [<arg2> ...]]
  [EXTRAS <arg1> [<arg2> ...]]
)
```

where the purpose of each argument should be clear from the help text for [gen_srproxy](#gen_srproxy). This command will automatically set up dependencies on the build of `gen_srproxy` and any file passed as input to its invocation. The output source file will be named `<OUTPUT_NAME>.cxx` and can be used in the definition of a library `MyClassProxy` like:

```
GenSRProxy(
  HEADER MyClass.h
  OUTPUT_NAME MyClassProxy
  TARGETNAME MyClass
  INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
  )

add_library(MyClassProxy SHARED MyClassProxy.cxx)
target_include_directories(MyClassProxy PUBLIC 
  ${CMAKE_CURRENT_SOURCE_DIR} 
  ${CMAKE_SOURCE_DIR}/src/include)
target_link_libraries(MyClassProxy PUBLIC SRProxy::BasicTypes)
```

*N.B.* that the produced library must link to `SRProxy::BasicTypes` which contains important template instantiations for the correct functioning of the generated Proxy classes.

## Generating a UPS product

If you configure the build with `-DEMIT_UPS_PRODUCT=ON` the build system will attempt to install the build targets in something that can be relocated to a UPS product database. It will use the result of `ups flavor` at configure time, and the result of `ups active | grep "^root"` to determine the build flavor and ROOT UPS dependency. It will create an install tree below CMAKE_INSTALL_PREFIX like:

```

<prefix>/srproxy/<srproxy_version>/<build_flavor>_<qualifiers>/ups/srproxy.table
<prefix>/srproxy/<srproxy_version>/<build_flavor>_<qualifiers>/../../<srproxy_version>.version/<build_flavor>_<qualifiers>
<prefix>/srproxy/<srproxy_version>/<build_flavor>_<qualifiers>/bin/gen_srproxy
<prefix>/srproxy/<srproxy_version>/<build_flavor>_<qualifiers>/lib/libSRProxy_BasicTypes.so
<prefix>/srproxy/<srproxy_version>/<build_flavor>_<qualifiers>/lib/cmake/SRProxy/SRProxyTargets.cmake
<prefix>/srproxy/<srproxy_version>/<build_flavor>_<qualifiers>/lib/cmake/SRProxy/SRProxyTargets-relwithdebinfo.cmake
<prefix>/srproxy/<srproxy_version>/<build_flavor>_<qualifiers>/lib/cmake/SRProxy/SRProxyConfigVersion.cmake
<prefix>/srproxy/<srproxy_version>/<build_flavor>_<qualifiers>/lib/cmake/SRProxy/SRProxyConfig.cmake
<prefix>/srproxy/<srproxy_version>/<build_flavor>_<qualifiers>/lib/cmake/SRProxy/GenSRProxy.cmake
<prefix>/srproxy/<srproxy_version>/<build_flavor>_<qualifiers>/bin/setup.SRProxy.sh
```