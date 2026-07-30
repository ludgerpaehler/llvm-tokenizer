cmake_minimum_required(VERSION 3.20)
project(structured-codec LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "structured-codec: found LLVM ${LLVM_PACKAGE_VERSION} (${LLVM_DIR})")

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

if(NOT LLVM_ENABLE_RTTI)
  add_compile_options(-fno-rtti)
endif()

# Library that encoder/decoder/tests share.
add_library(structured_codec_lib STATIC
  vocab.cpp
  encoder.cpp
  decoder.cpp
)
target_include_directories(structured_codec_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
llvm_map_components_to_libnames(STRUCTURED_CODEC_LLVM_LIBS support core irreader)
target_link_libraries(structured_codec_lib PUBLIC ${STRUCTURED_CODEC_LLVM_LIBS})

# CLI binary.
add_executable(structured-codec structured_codec.cpp)
target_link_libraries(structured-codec PRIVATE structured_codec_lib)

enable_testing()
add_subdirectory(test)
