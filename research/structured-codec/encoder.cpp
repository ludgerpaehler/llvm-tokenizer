#include "encoder.h"
#include "vocab.h"

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/Argument.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
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

// Walks a Module and emits the token stream.
class Encoder {
public:
  std::vector<uint32_t> encode(llvm::Module &M);

private:
  std::vector<uint32_t> tokens_;
  std::vector<llvm::Type *> type_table_;
  llvm::DenseMap<llvm::Type *, uint32_t> type_index_;

  uint64_t next_value_index_ = 0;
  llvm::DenseMap<llvm::Value *, uint64_t> value_index_;

  uint32_t internType(llvm::Type *T);
  void emitTypeDef(llvm::Type *T);
  void emitNameLiteral(llvm::StringRef name);
  uint64_t recordValue(llvm::Value *V);
  void emitFunction(llvm::Function &F);
};

uint32_t Encoder::internType(llvm::Type *T) {
  auto it = type_index_.find(T);
  if (it != type_index_.end()) return it->second;
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

void Encoder::emitNameLiteral(llvm::StringRef name) {
  tokens_.push_back(encodeTag(Tag::NAME));
  emitVarint(tokens_, name.size());
  for (char c : name) tokens_.push_back(encodeByte(static_cast<uint8_t>(c)));
}

uint64_t Encoder::recordValue(llvm::Value *V) {
  uint64_t idx = next_value_index_++;
  value_index_[V] = idx;
  return idx;
}

void Encoder::emitFunction(llvm::Function &F) {
  // Intern the function type FIRST (this emits its TYPEDEFs, including ret +
  // params, before FUNC_BEGIN).
  uint32_t fty_index = internType(F.getFunctionType());

  tokens_.push_back(encodeTag(Tag::FUNC_BEGIN));
  emitNameLiteral(F.getName());
  emitVarint(tokens_, fty_index);
  recordValue(&F);

  for (llvm::Argument &A : F.args()) {
    tokens_.push_back(encodeTag(Tag::ARG));
    emitNameLiteral(A.getName());
    recordValue(&A);
  }

  for (llvm::BasicBlock &BB : F) {
    tokens_.push_back(encodeTag(Tag::BLOCK_BEGIN));
    emitNameLiteral(BB.getName());
    recordValue(&BB);
    // Instructions emitted in T7.
  }

  tokens_.push_back(encodeTag(Tag::FUNC_END));
}

std::vector<uint32_t> Encoder::encode(llvm::Module &M) {
  tokens_.clear();
  type_table_.clear();
  type_index_.clear();
  next_value_index_ = 0;
  value_index_.clear();

  tokens_.push_back(encodeTag(Tag::MODULE_BEGIN));
  for (llvm::Function &F : M) {
    if (F.isDeclaration()) continue; // M0 ignores declarations
    emitFunction(F);
  }
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
