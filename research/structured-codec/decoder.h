#pragma once
#include <string>
namespace structured_codec {
// Decode the token-stream file at `in_path` and write an .ll file to `out_path`.
// Returns 0 on success, non-zero on error.
int decodeFile(const std::string &in_path, const std::string &out_path);
} // namespace structured_codec
