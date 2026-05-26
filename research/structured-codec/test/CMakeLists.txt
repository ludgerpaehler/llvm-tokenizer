add_executable(test_leb128 test_leb128.cpp)
target_link_libraries(test_leb128 PRIVATE structured_codec_lib)
add_test(NAME test_leb128 COMMAND test_leb128)
