
# GenSRProxy(
# [FLAT] 
# [VERBOSE]
# [VVERBOSE]
# [HEADER <arg>]
# [OUTPUT_NAME <arg>]
# [OUTPUT_PATH <arg>]
# [TARGETNAME <arg>]
# [PROLOG <arg>]
# [EPILOG <arg>]
# [EPILOG_FWD <arg>]
# [INCLUDE_DIRS <arg1> [<arg2> ...]]
# [DEPENDENCIES <arg1> [<arg2> ...]]
# [EXTRAS <arg1> [<arg2> ...]]
# [DEFINITIONS <arg1> [<arg2> ...]]
#)
function(GenSRProxy)

  set(options FLAT VERBOSE VVERBOSE)
  set(oneValueArgs HEADER OUTPUT_NAME OUTPUT_PATH TARGETNAME PROLOG EPILOG EPILOG_FWD DEFINITIONS)
  set(multiValueArgs INCLUDE_DIRS DEPENDENCIES EXTRAS)
  cmake_parse_arguments(OPTS 
                      "${options}" 
                      "${oneValueArgs}"
                      "${multiValueArgs}" ${ARGN})


  if(NOT DEFINED OPTS_HEADER)
    message(FATAL_ERROR "GenSRProxy did not recieve all required options: HEADER.")
  endif()

  if(NOT DEFINED OPTS_OUTPUT_NAME)
    message(FATAL_ERROR "GenSRProxy did not recieve all required options: OUTPUT_NAME.")
  endif()

  if(NOT DEFINED OPTS_TARGETNAME)
    message(FATAL_ERROR "GenSRProxy did not recieve all required options: TARGETNAME.")
  endif()

  SET(FLAT_ARG)
  if(OPTS_FLAT)
    SET(FLAT_ARG --flat)
  endif()

  SET(VERBOSE_ARG)
  if(OPTS_VERBOSE)
    SET(VERBOSE_ARG --verbose)
  endif()

  SET(VVERBOSE_ARG)
  if(OPTS_VVERBOSE)
    SET(VVERBOSE_ARG --vverbose)
  endif()

  SET(DEPENDENCIES ${HEADER})
  if(DEFINED OPTS_DEPENDENCIES)
    LIST(APPEND DEPENDENCIES ${OPTS_DEPENDENCIES})
  endif()

  SET(TARGET_ARG)
  if(DEFINED OPTS_TARGETNAME)
    SET(TARGET_ARG --target ${OPTS_TARGETNAME})
  endif()

  SET(PROLOG_ARG)
  if(DEFINED OPTS_PROLOG)
    SET(PROLOG_ARG --prolog ${OPTS_PROLOG})
    LIST(APPEND DEPENDENCIES ${OPTS_PROLOG})
  endif()

  SET(EPILOG_ARG)
  if(DEFINED OPTS_EPILOG)
    SET(EPILOG_ARG --epilog ${OPTS_EPILOG})
    LIST(APPEND DEPENDENCIES ${OPTS_EPILOG})
  endif()

  SET(EPILOG_FWD_ARG)
  if(DEFINED OPTS_EPILOG_FWD)
    SET(EPILOG_FWD_ARG --epilog-fwd ${OPTS_EPILOG_FWD})
    LIST(APPEND DEPENDENCIES ${OPTS_EPILOG_FWD})
  endif()

  SET(INCLUDE_ARG)
  if(DEFINED OPTS_INCLUDE_DIRS)
    STRING(REPLACE ";" ":" INCLUDE_PATH "${OPTS_INCLUDE_DIRS}")
    SET(INCLUDE_ARG --include-path ${INCLUDE_PATH})
  endif()

  SET(OUTPUT_PATH_ARG)
  if(DEFINED OPTS_OUTPUT_PATH)
    SET(OUTPUT_PATH_ARG --output-path ${OPTS_OUTPUT_PATH})
  endif()

  SET(DEFINITIONS_ARGS)
  if(DEFINED OPTS_DEFINITIONS)
    foreach(DEF ${OPTS_DEFINITIONS})
      LIST(APPEND DEFINITIONS_ARGS -D${DEF})
    endforeach()
  endif()

  add_custom_command(
    OUTPUT ${OPTS_OUTPUT_NAME}.cxx ${OPTS_OUTPUT_NAME}.h FwdDeclare.h
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND $<TARGET_FILE:gen_srproxy>
         ${FLAT_ARG} 
         -i ${OPTS_HEADER} 
         -o ${OPTS_OUTPUT_NAME}
         ${TARGET_ARG}
         ${INCLUDE_ARG}
         ${DEFINITIONS_ARGS}
         ${OUTPUT_PATH_ARG}
         ${EPILOG_ARG}
         ${EPILOG_FWD_ARG}
         ${PROLOG_ARG}
         ${EXTRAS_ARGS}
         ${VERBOSE_ARG}
         ${VVERBOSE_ARG}
         -od ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS gen_srproxy ${DEPENDENCIES})

endfunction(GenSRProxy)
