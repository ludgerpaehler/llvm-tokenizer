#include "decoder.h"
#include "vocab.h"

#include <llvm/IR/Argument.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace structured_codec {

class Decoder {
public:
  std::unique_ptr<llvm::Module> decode(const std::vector<uint32_t> &tokens,
                                       llvm::LLVMContext &ctx);

private:
  const std::vector<uint32_t> *tokens_ = nullptr;
  size_t pos_ = 0;
  llvm::LLVMContext *ctx_ = nullptr;
  llvm::Module *module_ = nullptr;

  std::vector<llvm::Type *> type_table_;
  std::vector<llvm::Value *> values_;

  Tag peekTag() const { return decodeTag((*tokens_)[pos_]); }
  Tag readTag() { return decodeTag((*tokens_)[pos_++]); }
  uint64_t readVar() { return readVarint(*tokens_, pos_); }
  std::string readNameLiteral();

  void readTypeDef();
  void readFunc();
};

std::string Decoder::readNameLiteral() {
  Tag t = readTag();
  if (t != Tag::NAME) {
    std::fprintf(stderr, "decode: expected NAME tag, got %u\n",
                 static_cast<unsigned>(t));
    std::exit(2);
  }
  uint64_t n = readVar();
  std::string s;
  s.resize(static_cast<size_t>(n));
  for (size_t i = 0; i < n; ++i)
    s[i] = static_cast<char>(decodeByte((*tokens_)[pos_++]));
  return s;
}

void Decoder::readTypeDef() {
  // TYPEDEF tag already consumed by the caller.
  TypeKind kind = decodeTypeKind((*tokens_)[pos_++]);
  switch (kind) {
  case TypeKind::INTEGER: {
    uint64_t bits = readVar();
    type_table_.push_back(
        llvm::IntegerType::get(*ctx_, static_cast<unsigned>(bits)));
    return;
  }
  case TypeKind::VOID:
    type_table_.push_back(llvm::Type::getVoidTy(*ctx_));
    return;
  case TypeKind::FUNCTION: {
    uint64_t ret_idx = readVar();
    uint64_t nparams = readVar();
    std::vector<llvm::Type *> params;
    params.reserve(static_cast<size_t>(nparams));
    for (uint64_t i = 0; i < nparams; ++i)
      params.push_back(type_table_[static_cast<size_t>(readVar())]);
    uint64_t vararg = readVar();
    type_table_.push_back(llvm::FunctionType::get(
        type_table_[static_cast<size_t>(ret_idx)], params, vararg != 0));
    return;
  }
  default:
    std::fprintf(stderr, "decode: unknown TypeKind %u\n",
                 static_cast<unsigned>(kind));
    std::exit(2);
  }
}

void Decoder::readFunc() {
  // FUNC_BEGIN tag already consumed by the caller.
  std::string name = readNameLiteral();
  uint64_t fty_idx = readVar();
  auto *FTy = llvm::cast<llvm::FunctionType>(
      type_table_[static_cast<size_t>(fty_idx)]);
  llvm::Function *F = llvm::Function::Create(
      FTy, llvm::GlobalValue::ExternalLinkage, name, module_);
  values_.push_back(F);

  // Walk ARG / BLOCK_BEGIN / INSTR / FUNC_END / interleaved TYPEDEF records
  // (a TYPEDEF can appear at any point in the stream when the encoder needs to
  // intern a new type; treat it like the top-level case).
  llvm::Function::arg_iterator arg_it = F->arg_begin();
  llvm::BasicBlock *current_bb = nullptr;
  while (true) {
    Tag t = readTag();
    switch (t) {
    case Tag::TYPEDEF:
      readTypeDef();
      break;
    case Tag::ARG: {
      std::string an = readNameLiteral();
      if (arg_it == F->arg_end()) {
        std::fprintf(stderr, "decode: more ARG records than parameters\n");
        std::exit(2);
      }
      arg_it->setName(an);
      values_.push_back(&*arg_it);
      ++arg_it;
      break;
    }
    case Tag::BLOCK_BEGIN: {
      std::string bn = readNameLiteral();
      current_bb = llvm::BasicBlock::Create(*ctx_, bn, F);
      values_.push_back(current_bb);
      break;
    }
    case Tag::INSTR:
      // T9 fills this in.
      std::fprintf(stderr, "decode: INSTR not yet implemented (Task 9)\n");
      std::exit(2);
    case Tag::FUNC_END:
      return;
    default:
      std::fprintf(stderr, "decode: unexpected tag %u inside function\n",
                   static_cast<unsigned>(t));
      std::exit(2);
    }
  }
}

std::unique_ptr<llvm::Module>
Decoder::decode(const std::vector<uint32_t> &tokens, llvm::LLVMContext &ctx) {
  tokens_ = &tokens;
  pos_ = 0;
  ctx_ = &ctx;
  auto M = std::make_unique<llvm::Module>("structured-codec", ctx);
  module_ = M.get();
  type_table_.clear();
  values_.clear();

  if (readTag() != Tag::MODULE_BEGIN) {
    std::fprintf(stderr, "decode: expected MODULE_BEGIN\n");
    return nullptr;
  }
  while (true) {
    Tag t = readTag();
    if (t == Tag::MODULE_END) break;
    if (t == Tag::TYPEDEF) { readTypeDef(); continue; }
    if (t == Tag::FUNC_BEGIN) { readFunc(); continue; }
    std::fprintf(stderr, "decode: unexpected top-level tag %u\n",
                 static_cast<unsigned>(t));
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
