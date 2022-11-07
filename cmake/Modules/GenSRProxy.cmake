function(GenSRProxy)

  set(options FLAT)
  set(oneValueArgs HEADER OUTPUT_NAME OUTPUT_PATH TARGETNAME EPILOG)
  set(multiValueArgs INCLUDE_DIRS DEPENDENCIES)
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

  SET(DEPENDENCIES ${HEADER})
  if(DEFINED OPTS_DEPENDENCIES)
    LIST(APPEND DEPENDENCIES ${OPTS_DEPENDENCIES})
  endif()

  SET(TARGET_ARG)
  if(DEFINED OPTS_TARGETNAME)
    SET(TARGET_ARG --target ${OPTS_TARGETNAME})
  endif()

  SET(EPILOG_ARG)
  if(DEFINED OPTS_EPILOG)
    SET(EPILOG_ARG --epilog-fwd ${OPTS_EPILOG})
  endif()

  SET(INCLUDE_ARG)
  if(DEFINED OPTS_INCLUDE_DIRS)
    STRING(REPLACE ";" ":" INCLUDE_PATH "${OPTS_INCLUDE_DIRS}")
    SET(INCLUDE_ARG --include-path ${INCLUDE_PATH})
  endif()

  SET(OPTS_OUTPUT_PATH_ARG)
  if(DEFINED OPTS_OUTPUT_PATH)
    SET(OPTS_OUTPUT_PATH_ARG --output-path ${OPTS_OUTPUT_PATH})
  endif()

  message(STATUS "[GenSRProxy] -------")
  message(STATUS "[GenSRProxy] Outputs: ${OPTS_OUTPUT_NAME}.cxx ${OPTS_OUTPUT_NAME}.h FwdDeclare.h")
  message(STATUS "[GenSRProxy] WorkDir: ${CMAKE_CURRENT_BINARY_DIR}")
  message(STATUS "[GenSRProxy] Command: gen_srproxy ${FLAT_ARG} -i ${OPTS_HEADER} -o ${OPTS_OUTPUT_NAME} ${TARGET_ARG} ${INCLUDE_ARG} ${OPTS_OUTPUT_PATH_ARG}")
  message(STATUS "[GenSRProxy] Dependencies: ${DEPENDENCIES}")
  message(STATUS "[GenSRProxy] -------")

  add_custom_command(
    OUTPUT ${OPTS_OUTPUT_NAME}.cxx ${OPTS_OUTPUT_NAME}.h FwdDeclare.h
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:GenSRProxy>
         ${FLAT_ARG} 
         -i ${OPTS_HEADER} 
         -o ${OPTS_OUTPUT_NAME} 
         ${TARGET_ARG}
         ${INCLUDE_ARG} 
         ${OPTS_OUTPUT_PATH_ARG} 
    DEPENDS GenSRProxy ${DEPENDENCIES})

endfunction(GenSRProxy)
