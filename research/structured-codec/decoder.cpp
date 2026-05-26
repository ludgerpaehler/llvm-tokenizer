#include "decoder.h"
#include "vocab.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdio>
#include <fstream>
#include <memory>
#include <vector>

namespace structured_codec {

class Decoder {
public:
  std::unique_ptr<llvm::Module> decode(const std::vector<uint32_t> &tokens,
                                       llvm::LLVMContext &ctx);
};

std::unique_ptr<llvm::Module>
Decoder::decode(const std::vector<uint32_t> &tokens, llvm::LLVMContext &ctx) {
  auto M = std::make_unique<llvm::Module>("structured-codec", ctx);
  size_t pos = 0;
  if (pos >= tokens.size() || decodeTag(tokens[pos++]) != Tag::MODULE_BEGIN) {
    std::fprintf(stderr, "decode: expected MODULE_BEGIN at start\n");
    return nullptr;
  }
  // TODO(M0 task 8+): consume TYPEDEFs, functions, until MODULE_END.
  if (pos >= tokens.size() || decodeTag(tokens[pos++]) != Tag::MODULE_END) {
    std::fprintf(stderr, "decode: expected MODULE_END\n");
    return nullptr;
  }
  return M;
}

static std::vector<uint32_t> readTokens(const std::string &path) {
  std::ifstream f(path);
  std::vector<uint32_t> tokens;
  uint32_t t;
  while (f >> t) tokens.push_back(t);
  return tokens;
}

int decodeFile(const std::string &in_path, const std::string &out_path) {
  llvm::LLVMContext ctx;
  auto tokens = readTokens(in_path);
  Decoder dec;
  auto M = dec.decode(tokens, ctx);
  if (!M) return 1;
  if (llvm::verifyModule(*M, &llvm::errs())) {
    std::fprintf(stderr, "decode: verifyModule failed\n");
    return 1;
  }
  std::error_code ec;
  llvm::raw_fd_ostream out(out_path, ec);
  if (ec) {
    std::fprintf(stderr, "decode: cannot open '%s': %s\n", out_path.c_str(),
                 ec.message().c_str());
    return 1;
  }
  M->print(out, /*AAW*/ nullptr);
  return 0;
}

} // namespace structured_codec
