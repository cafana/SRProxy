function(GenSRProxy)

  set(options FLAT)
  set(oneValueArgs HEADER OUTPUT_NAME OUTPUT_PATH TARGETNAME EPILOG EPILOG_FWD)
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
  SET(TARGET_ARG_STR)
  if(DEFINED OPTS_TARGETNAME)
    SET(TARGET_ARG --target ${OPTS_TARGETNAME})
    STRING(REPLACE ";" " " TARGET_ARG_STR "${TARGET_ARG}")
  endif()

  SET(EPILOG_ARG)
  SET(EPILOG_ARG_STR)
  if(DEFINED OPTS_EPILOG)
    SET(EPILOG_ARG --epilog ${OPTS_EPILOG})
    STRING(REPLACE ";" " " EPILOG_ARG_STR "${EPILOG_ARG}")
  endif()

  SET(EPILOG_FWD_ARG)
  SET(EPILOG_FWD_ARG_STR)
  if(DEFINED OPTS_EPILOG_FWD)
    SET(EPILOG_FWD_ARG --epilog-fwd ${OPTS_EPILOG_FWD})
    STRING(REPLACE ";" " " EPILOG_FWD_ARG_STR "${EPILOG_FWD_ARG}")
  endif()

  SET(INCLUDE_ARG)
  SET(INCLUDE_ARG_STR)
  if(DEFINED OPTS_INCLUDE_DIRS)
    STRING(REPLACE ";" ":" INCLUDE_PATH "${OPTS_INCLUDE_DIRS}")
    SET(INCLUDE_ARG --include-path ${INCLUDE_PATH})
    STRING(REPLACE ";" " " INCLUDE_ARG_STR "${INCLUDE_ARG}")
  endif()

  SET(OUTPUT_PATH_ARG)
  SET(OUTPUT_PATH_ARG_STR)
  if(DEFINED OPTS_OUTPUT_PATH)
    SET(OUTPUT_PATH_ARG --output-path ${OPTS_OUTPUT_PATH})
    STRING(REPLACE ";" " " OUTPUT_PATH_ARG_STR "${OUTPUT_PATH_ARG}")
  endif()

  message(STATUS "[GenSRProxy] -------")
  message(STATUS "[GenSRProxy] Outputs: ${OPTS_OUTPUT_NAME}.cxx ${OPTS_OUTPUT_NAME}.h FwdDeclare.h")
  message(STATUS "[GenSRProxy] WorkDir: ${CMAKE_CURRENT_BINARY_DIR}")
  message(STATUS "[GenSRProxy] Command: gen_srproxy ${FLAT_ARG} -i ${OPTS_HEADER} -o ${OPTS_OUTPUT_NAME} ${TARGET_ARG_STR} ${INCLUDE_ARG_STR} ${OUTPUT_PATH_ARG_STR} ${EPILOG_ARG_STR} ${EPILOG_FWD_ARG_STR}")
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
         ${OUTPUT_PATH_ARG}
         ${EPILOG_ARG}
         ${EPILOG_FWD_ARG}
    DEPENDS GenSRProxy ${DEPENDENCIES})

endfunction(GenSRProxy)
