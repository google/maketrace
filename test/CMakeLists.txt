set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -pedantic")

include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_BINARY_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/protoc-qt/runtime/include)

find_file(gtest_source_dir NAMES src/gtest)
add_library(test_common OBJECT
  ${gtest_source_dir}/src/gtest-all.cc
  ${gtest_source_dir}/src/gtest_main.cc

  logging_env.cc
)
target_include_directories(test_common PUBLIC ${gtest_source_dir})

macro(test test_name)
  add_executable(${test_name} ${test_name}.cc
    $<TARGET_OBJECTS:tracerlib>
    $<TARGET_OBJECTS:test_common>)
  target_link_libraries(${test_name}
    Qt5
    ${CMAKE_THREAD_LIBS_INIT}
    ${GLOG_LIBRARY}
    ${GFLAGS_LIBRARY}
    ${UNWIND_LIBRARY}
    ${LZMA_LIBRARY}
    ${PROTOBUF_LIBRARY}
    protobuf_qt
  )
  add_test(${test_name} ${test_name})
endmacro()

test(tracer_test)
