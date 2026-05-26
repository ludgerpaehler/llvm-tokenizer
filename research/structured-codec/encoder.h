#pragma once
#include <string>
namespace structured_codec {
// Encode the IR at `in_path` into a token-stream file at `out_path`.
// Returns 0 on success, non-zero on error.
int encodeFile(const std::string &in_path, const std::string &out_path);
} // namespace structured_codec
