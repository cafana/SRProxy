# SRProxy


(Please also see [the general CAFAna `README`](https://github.com/cafana) for more information.)

`SRProxy` is a toolkit for fast reads of `StandardRecord` objects from ROOT files.
It can read two kinds of files:
* "Structured" (or traditional) CAFs, in which there is one `StandardRecord` object per entry
* "Flat" CAFs, in which a `StandardRecord` object is 'flattened' during serialization into basic ROOT types, 
  and the structure is maintained in the branch names only.

Such CAFs are written by "CAF-maker" software maintained by the experiments that use CAFs as their analysis files.

When used, `SRProxy` provides automatic compilation-time deduction of which branches within the `StandardRecord` object
need to be enabled when reading from the file.
Any unused branches are disabled.
For complicated `StandardRecord` objects, this can result in speedups of several orders of magnitude. 

## Usage
`SRProxy` needs to be templated over a concrete `StandardRecord` type that contains
the relevant fields for the user's needs.
In-practice examples include the implementations by [SBN](https://github.com/SBNSoftware/sbnana/tree/develop/sbnana/CAFAna)
and [DUNE](https://github.com/DUNE/lblpwgtools/tree/master/CAFAna).

It would be nice to have a technical digest of how to do this here, but in the meantime, 
please contact the [CAFAna librarian](https://github.com/orgs/cafana/teams/librarian)
and we can discuss your use case.

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
  -o|--output <filename stub>    : Output filename stub

Optional arguments:
  -I <path>                      : A directory to add to the include path
  -D <symbol>[=val]              : A symbol definition, with optional value, to the interpreter before parsing

  --flat                         : Generate a 'flat' file reader rather than the objectified proxy class

  --order-alphabetically         : Emit datamembers in alphabetic, rather than declaration, order.
  
  -p|--include-path <path1[:p2]> : A PATH-like colon-separate list of directories to add to the include path
  -op|--output-path <path>       : A path to prepend to include statements in generated headers
  -od|--output-dir <path>        : The directory to write generated files to
  --prolog <file path>           : A file to include before the generated proxy class defintion
  --epilog <file path>           : A file to include after the generated proxy class definition
  --epilog-fwd <file path>       : A file to include after the list of generated forward declarations
  --extra <classname> <file>     : A file to include in the definition of the proxy class for class <classname>

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
