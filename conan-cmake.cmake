#########################################################################################
#  Configure with Conan
#
if (CMAKE_BUILD_TYPE)
  set(CONAN_BUILD_TYPES ${CMAKE_BUILD_TYPE})
  message(STATUS "Single Config CMAKE_BUILD_TYPE ${CONAN_BUILD_TYPES}")
else()
  set(CONAN_BUILD_TYPES ${CMAKE_CONFIGURATION_TYPES})
  message(STATUS "Multi Config CMAKE_CONFIGURATION_TYPES ${CONAN_BUILD_TYPES}")
endif()

find_program(CONAN_CMD conan)
if(NOT CONAN_CMD)
    message(FATAL_ERROR "Conan executable not found! Please install conan.")
endif()

file(TO_NATIVE_PATH "${CONAN_CMD}" CONAN_CMD)
file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}" CONAN_SOURCE_FOLDER)
file(TO_NATIVE_PATH "${CMAKE_BINARY_DIR}" CONAN_INSTALL_FOLDER)

foreach(CONAN_BUILD_TYPE IN LISTS CONAN_BUILD_TYPES)
    set(CONAN_ARGS "install")
    list(APPEND CONAN_ARGS "-if=${CONAN_INSTALL_FOLDER}")
    list(APPEND CONAN_ARGS "--build" "outdated")
    list(APPEND CONAN_ARGS "-s" "build_type=${CONAN_BUILD_TYPE}")
    list(APPEND CONAN_ARGS "${CONAN_SOURCE_FOLDER}")

    execute_process(COMMAND ${CONAN_CMD} ${CONAN_ARGS}
                    COMMAND_ECHO STDOUT
                    RESULT_VARIABLE return_code
                    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

    if(NOT "${return_code}" STREQUAL "0")
        message(FATAL_ERROR "Conan install failed='${return_code}'")
    endif()
endforeach()

# Add conan paths
include(${CMAKE_BINARY_DIR}/conan_paths.cmake)

# grab Ctl module from ctl-cmake conan package
#include(CtlDefaults)
#
#########################################################################################
