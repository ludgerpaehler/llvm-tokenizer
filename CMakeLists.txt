cmake_minimum_required(VERSION 3.20.0)
project(llvm-tokenizer)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set (CMAKE_CXX_STANDARD 17)

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

if(NOT LLVM_ENABLE_RTTI)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

add_executable(llvm-tokenizer llvm-tokenizer.cpp)

llvm_map_components_to_libnames(llvm_libs support core irreader)

target_link_libraries(llvm-tokenizer ${llvm_libs})

install(TARGETS llvm-tokenizer DESTINATION bin)

add_subdirectory(test)

