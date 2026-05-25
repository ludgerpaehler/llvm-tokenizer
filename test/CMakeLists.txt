configure_file(lit.site.cfg.py.in lit.site.cfg.py @ONLY)

# Prefer the llvm-lit co-installed with the build's LLVM over a system-wide lit.
find_program(LLVM_LIT NAMES llvm-lit lit
             HINTS ${LLVM_TOOLS_BINARY_DIR}
             DOC "Path to the lit test runner")

if(NOT LLVM_LIT)
  message(WARNING
    "lit not found; the 'check-llvm-tokenizer' target will be unavailable. "
    "Install it (e.g. 'pip install lit' or 'uv tool install lit').")
else()
  add_custom_target(check-llvm-tokenizer
    COMMAND "${LLVM_LIT}" -v "${CMAKE_CURRENT_BINARY_DIR}"
    DEPENDS llvm-tokenizer
    USES_TERMINAL)
endif()
