cmake_minimum_required(VERSION 3.9.4)
project(podgen)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_MODULE_PATH})
include(conan-cmake)

find_package(CapnProto REQUIRED CONFIG)
include(CtlCapnProto)


# ---------------------------------------------------------------------------------------
# Header only target for templates folder + defs.hpp
# ---------------------------------------------------------------------------------------
#add_library(spdlog_header_only INTERFACE)
#add_library(spdlog::spdlog_header_only ALIAS spdlog_header_only)
#
#target_include_directories(spdlog_header_only INTERFACE
#    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
#    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
#target_link_libraries(spdlog_header_only INTERFACE Threads::Threads)



ctl_capnp(CAPNP_SRCS CAPNP_HDRS include/pod.capnp)

add_executable(podgen
    src/main.cpp
    src/PodGenerator.cpp
    src/PodGenerator.hpp
    src/reserved_words.hpp
    src/schema.cpp
    src/schema.hpp
    include/defs.hpp
    include/timestamp.convert.hpp
    ${CAPNP_SRCS}
    ${CAPNP_HDRS}
)

target_include_directories(podgen PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include)
target_link_libraries(podgen PRIVATE CapnProto::capnp CapnProto::capnpc)

#file(GLOB include_files "${CMAKE_CURRENT_SOURCE_DIR}/include/*")
#add_custom_target(copy_templates
#    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${include_files} ${CMAKE_CURRENT_BINARY_DIR}/include
#)
#add_dependencies(podgen copy_templates)

#function(capnp_generate_pod_cpp SOURCES HEADERS)
function(capnp_generate_pod_cpp target_name)
    set(base_dir "${CMAKE_CURRENT_BINARY_DIR}/pod_autogen")

    set(CAPNPC_IMPORT_DIRS "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/include")
    set(CAPNPC_OUTPUT_DIR "${base_dir}")
#    ctl_capnp(SOURCES HEADERS ${ARGN})
    ctl_capnp(CAPNP_SRCS CAPNP_HDRS ${ARGN})

    if(NOT CTL_CAPNP_INCLUDE_DIRECTORY)
        get_target_property(CTL_CAPNP_INCLUDE_DIRECTORY CapnProto::capnp INTERFACE_INCLUDE_DIRECTORIES)
    endif()

    set(outputs)
    foreach (src ${ARGN})
        # remove ".capnp" file extension and generate fully qualified output names
        string(REGEX REPLACE "\\.capnp$" "" src_base "${src}")
        list(APPEND outputs "${base_dir}/${src_base}.pod.hpp")
        list(APPEND outputs "${base_dir}/${src_base}.convert.hpp" "${base_dir}/${src_base}.convert.cpp")
#        list(APPEND HEADERS "${base_dir}/${src_base}.pod.hpp")
#        list(APPEND SOURCES "${base_dir}/${src_base}.convert.hpp" "${base_dir}/${src_base}.convert.cpp")
    endforeach ()

    add_custom_command(OUTPUT ${outputs}
            COMMAND $<TARGET_FILE:podgen> ${ARGN} -c "${CTL_CAPNP_INCLUDE_DIRECTORY}" -t "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/include" -o "${base_dir}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            DEPENDS ${ARGN} $<TARGET_FILE:podgen>
            VERBATIM)

    add_library(${target_name} STATIC EXCLUDE_FROM_ALL ${outputs} ${CAPNP_SRCS})
    target_include_directories(${target_name} PUBLIC "${base_dir}" "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/include")
    target_link_libraries(${target_name} PUBLIC CapnProto::capnp)
#    set_source_files_properties(${${SOURCES}} ${${HEADERS}} PROPERTIES GENERATED TRUE)
#    set(${SOURCES} ${${SOURCES}} PARENT_SCOPE)
#    set(${HEADERS} ${${HEADERS}} PARENT_SCOPE)
endfunction()

add_subdirectory(example)
add_subdirectory(test)
