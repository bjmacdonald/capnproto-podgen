find_package(Eigen3 REQUIRED)

capnp_generate_pod_cpp(test_pod include.capnp test.capnp)

#capnp_generate_pod_cpp(SOURCES HEADERS include.capnp test.capnp)
#add_library(test_pod STATIC EXCLUDE_FROM_ALL ${SOURCES} ${HEADERS})
target_include_directories(test_pod PUBLIC ${EIGEN3_INCLUDE_DIRS})

add_executable(podgen_test EXCLUDE_FROM_ALL podgen_test.cpp vector.convert.cpp)
target_link_libraries(podgen_test PRIVATE test_pod)