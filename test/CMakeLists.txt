configure_file(lit.site.cfg.py.in lit.site.cfg.py @ONLY)

add_custom_target(check-llvm-tokenizer
COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/run-tests.py" "${CMAKE_CURRENT_BINARY_DIR}" -v DEPENDS llvm-tokenizer)

