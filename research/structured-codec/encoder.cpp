#include "encoder.h"
#include "vocab.h"

#include <llvm/ADT/DenseMap.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>

namespace structured_codec {

// Walks a Module and emits the token stream. Filled out across Tasks 5-7.
class Encoder {
public:
  std::vector<uint32_t> encode(llvm::Module &M);

private:
  std::vector<uint32_t> tokens_;
  std::vector<llvm::Type *> type_table_;          // index -> Type*
  llvm::DenseMap<llvm::Type *, uint32_t> type_index_;

  // Returns the table index for T, interning recursively if new.
  // Emits a TYPEDEF record the first time T is seen.
  uint32_t internType(llvm::Type *T);

  // Appends a TYPEDEF record for T (subtypes must already be interned).
  void emitTypeDef(llvm::Type *T);
};

uint32_t Encoder::internType(llvm::Type *T) {
  auto it = type_index_.find(T);
  if (it != type_index_.end()) return it->second;

  // For composite types, intern subtypes BEFORE assigning this type's index
  // so their TYPEDEF records precede ours and their indices are lower.
  if (auto *FT = llvm::dyn_cast<llvm::FunctionType>(T)) {
    (void)internType(FT->getReturnType());
    for (llvm::Type *PT : FT->params()) (void)internType(PT);
  }

  uint32_t idx = static_cast<uint32_t>(type_table_.size());
  type_table_.push_back(T);
  type_index_[T] = idx;
  emitTypeDef(T);
  return idx;
}

void Encoder::emitTypeDef(llvm::Type *T) {
  if (auto *FT = llvm::dyn_cast<llvm::FunctionType>(T)) {
    // Subtypes already interned by internType; look them up.
    uint32_t ret_idx = type_index_[FT->getReturnType()];
    std::vector<uint32_t> param_idxs;
    param_idxs.reserve(FT->getNumParams());
    for (llvm::Type *PT : FT->params()) param_idxs.push_back(type_index_[PT]);

    tokens_.push_back(encodeTag(Tag::TYPEDEF));
    tokens_.push_back(encodeTypeKind(TypeKind::FUNCTION));
    emitVarint(tokens_, ret_idx);
    emitVarint(tokens_, param_idxs.size());
    for (uint32_t pi : param_idxs) emitVarint(tokens_, pi);
    emitVarint(tokens_, FT->isVarArg() ? 1 : 0);
    return;
  }
  if (auto *IT = llvm::dyn_cast<llvm::IntegerType>(T)) {
    tokens_.push_back(encodeTag(Tag::TYPEDEF));
    tokens_.push_back(encodeTypeKind(TypeKind::INTEGER));
    emitVarint(tokens_, IT->getBitWidth());
    return;
  }
  if (T->isVoidTy()) {
    tokens_.push_back(encodeTag(Tag::TYPEDEF));
    tokens_.push_back(encodeTypeKind(TypeKind::VOID));
    return;
  }
  std::fprintf(stderr,
               "encode: M0 cannot yet handle type kind %u (added in M1)\n",
               static_cast<unsigned>(T->getTypeID()));
  std::exit(2);
}

std::vector<uint32_t> Encoder::encode(llvm::Module &M) {
  tokens_.clear();
  type_table_.clear();
  type_index_.clear();
  tokens_.push_back(encodeTag(Tag::MODULE_BEGIN));
  // (Functions walked in T6.) No types are interned yet.
  (void)M;
  tokens_.push_back(encodeTag(Tag::MODULE_END));
  return std::move(tokens_);
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
