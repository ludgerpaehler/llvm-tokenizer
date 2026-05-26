#include "encoder.h"
#include "vocab.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdio>
#include <fstream>
#include <vector>

namespace structured_codec {

// Walks a Module and emits the token stream. Filled out across Tasks 5-7.
class Encoder {
public:
  std::vector<uint32_t> encode(llvm::Module &M);
};

std::vector<uint32_t> Encoder::encode(llvm::Module &M) {
  std::vector<uint32_t> tokens;
  tokens.push_back(encodeTag(Tag::MODULE_BEGIN));
  // TODO(M0 task 5): TYPEDEF table, function walk.
  tokens.push_back(encodeTag(Tag::MODULE_END));
  (void)M;
  return tokens;
}

static int writeTokens(const std::vector<uint32_t> &tokens,
                       const std::string &path) {
  std::ofstream f(path);
  if (!f) {
    std::fprintf(stderr, "encode: cannot open output '%s'\n", path.c_str());
    return 1;
  }
  for (size_t i = 0; i < tokens.size(); ++i) {
    if (i) f << ' ';
    f << tokens[i];
  }
  // Track C convention: no trailing newline required, but harmless.
  return 0;
}

int encodeFile(const std::string &in_path, const std::string &out_path) {
  llvm::LLVMContext ctx;
  llvm::SMDiagnostic err;
  auto M = llvm::parseIRFile(in_path, err, ctx);
  if (!M) {
    err.print("structured-codec encode", llvm::errs());
    return 1;
  }
  Encoder enc;
  auto tokens = enc.encode(*M);
  return writeTokens(tokens, out_path);
}

} // namespace structured_codec
