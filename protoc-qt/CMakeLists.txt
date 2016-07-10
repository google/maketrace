include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/runtime/include)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(GENERATOR_SOURCES
  compiler/cpp_enum.cc
  compiler/cpp_enum_field.cc
  compiler/cpp_extension.cc
  compiler/cpp_field.cc
  compiler/cpp_file.cc
  compiler/cpp_generator.cc
  compiler/cpp_helpers.cc
  compiler/cpp_message.cc
  compiler/cpp_message_field.cc
  compiler/cpp_primitive_field.cc
  compiler/cpp_service.cc
  compiler/cpp_string_field.cc
)

add_library(generator STATIC ${GENERATOR_SOURCES})

add_executable(protoc-qt compiler/cli_main.cc)
target_link_libraries(protoc-qt
  generator
  Qt5::Core
  ${PROTOBUF_PROTOC_LIBRARY}
  ${PROTOBUF_LIBRARY}
)

set(RUNTIME_SOURCES
  runtime/generated_message_reflection.cc
)

add_library(protobuf_qt ${RUNTIME_SOURCES})
target_link_libraries(protobuf_qt
  Qt5::Core
  ${PROTOBUF_LIBRARY}
)
