project(maketrace)
cmake_minimum_required(VERSION 2.8)

find_package(gflags REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Protobuf REQUIRED)
find_package(Qt5Concurrent)
find_package(Qt5Core)
pkg_check_modules(GLOG REQUIRED libglog)
find_library(GFLAGS_LIBRARY gflags)
find_package(Threads)

add_definitions(${QT_DEFINITIONS})
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${gflags_INCLUDE_DIR})
include_directories(${PROTOBUF_INCLUDE_DIRS})
include_directories(${GLOG_INCLUDE_DIRS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -pedantic")

add_subdirectory(protoc-qt)
set(PROTOBUF_PROTOC_EXECUTABLE protoc-qt)

include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_BINARY_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/protoc-qt/runtime/include)

set(HEADERS
  src/common.h
  src/graph.h
  src/make_unique.h

  src/utils/logging.h
  src/utils/recordfile.h
  src/utils/stl.h
)

set(SOURCES
  src/fromapt.cc
  src/installedfilesreader.cc
  src/main.cc
  src/memory.cc
  src/reference.cc
  src/toolsearchpath.cc
  src/tracer.cc
  src/tracereader.cc

  src/analysis/buildtargetgen.cc
  src/analysis/configure.cc
  src/analysis/gccbuildtargetgen.cc
  src/analysis/install.cc
  src/analysis/make.cc
  src/analysis/staticlinkbuildtargetgen.cc
  src/analysis/targetmatchnode.cc
  src/analysis/tracenode.cc

  src/gen/bazel/buildwriter.cc
  src/gen/bazel/generator.cc
  src/gen/bazel/label.cc
  src/gen/bazel/rule.cc
  # src/gen/ninja/ninjagenerator.cc

  src/utils/path.cc
  src/utils/recursive_copy.cc
  src/utils/str.cc
  src/utils/subcommand.cc
)

set(PROTOS
  proto/tracer.proto
  src/gen/bazel/build.proto
)

protobuf_generate_cpp(PROTO_SOURCES PROTO_HDRS ${PROTOS})

add_executable(tracer
  ${HEADERS}
  ${SOURCES}
  ${PROTO_HEADERS}
  ${PROTO_SOURCES}
  ${TRACER_SOURCES}
)
target_link_libraries(tracer
  Qt5::Core
  Qt5::Concurrent
  ${CMAKE_THREAD_LIBS_INIT}
  ${GFLAGS_LIBRARY}
  ${GLOG_LIBRARIES}
  ${PROTOBUF_LIBRARY}
  protobuf_qt
)
