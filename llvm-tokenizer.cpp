#include "llvm/Support/CommandLine.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/ScopedPrinter.h>
#include <llvm/Support/SourceMgr.h>

#include <unordered_map>
#include <vector>

using namespace llvm;

static cl::opt<std::string> InputFilename(cl::Positional,
                                          cl::desc("Input Bitcode/Textual IR"),
                                          cl::init("-"));

static ExitOnError ExitOnErr("llvm-tokenizer error: ");

enum TokenType {
  PaddingToken,
  InstructionOperandToken,
  ConstantOperandToken,
  BasicBlockOperandToken,
  GlobalValueOperandToken,
  MetadataAsValueOperandToken,
  InlineASMOperandToken,
  ArgumentOperandToken,
  UnknownOperandToken,
  OpcodeToken
};

StringRef GetTokenTypeName(TokenType TypeInput) {
  switch (TypeInput) {
  case TokenType::PaddingToken:
    return "padding";
  case TokenType::InstructionOperandToken:
    return "instruction_operand";
  case TokenType::ConstantOperandToken:
    return "constant_operand";
  case TokenType::BasicBlockOperandToken:
    return "basic_block_operand";
  case TokenType::GlobalValueOperandToken:
    return "global_value_operand";
  case TokenType::MetadataAsValueOperandToken:
    return "metadata_as_value_operand";
  case TokenType::InlineASMOperandToken:
    return "inline_ASM_operand";
  case TokenType::ArgumentOperandToken:
    return "argument_operand";
  case TokenType::UnknownOperandToken:
    return "unknown_operand";
  case TokenType::OpcodeToken:
    return "opcode";
  }
  return "unknown_token";
}

union TokenData {
  unsigned Opcode;
  size_t ReferencedInstructionIndex;
};

struct Token {
  TokenType Type;
  TokenData Data;
  size_t InstructionIndex;

  Token(TokenType _Type, size_t _InstructionIndex)
      : Type(_Type), InstructionIndex(_InstructionIndex) {}
  Token(TokenType _Type, size_t _InstructionIndex, unsigned Opcode)
      : Type(_Type), InstructionIndex(_InstructionIndex) {
    Data.Opcode = Opcode;
  }
};

Token processOperand(
    Value *Operand, size_t InstructionIndex,
    std::unordered_map<Value *, size_t> &InstructionIndexMapping) {
  if (Instruction *I = dyn_cast<Instruction>(Operand)) {
    Token InstructionOperandToken =
        Token(TokenType::InstructionOperandToken, InstructionIndex);
    auto InstructionIndexMappingItr =
        InstructionIndexMapping.find(static_cast<Value *>(I));
    if (InstructionIndexMappingItr != InstructionIndexMapping.end()) {
      // TODO(boomanaiden154): See how often this case occurs and find out a
      // better way to deal with it.
      InstructionOperandToken.Data.ReferencedInstructionIndex = 0;
    } else {
      InstructionOperandToken.Data.ReferencedInstructionIndex =
          InstructionIndexMappingItr->second;
    }
    return InstructionOperandToken;
    return Token(TokenType::InstructionOperandToken, InstructionIndex);
  } else if (auto *ConstantOperand = llvm::dyn_cast<llvm::Constant>(Operand)) {
    return Token(TokenType::ConstantOperandToken, InstructionIndex);
  } else if (const BasicBlock *BB = dyn_cast<BasicBlock>(Operand)) {
    return Token(TokenType::BasicBlockOperandToken, InstructionIndex);
  } else if (const GlobalValue *GV = dyn_cast<GlobalValue>(Operand)) {
    return Token(TokenType::GlobalValueOperandToken, InstructionIndex);
  } else if (const MetadataAsValue *V = dyn_cast<MetadataAsValue>(Operand)) {
    return Token(TokenType::MetadataAsValueOperandToken, InstructionIndex);
  } else if (isa<InlineAsm>(Operand)) {
    return Token(TokenType::InlineASMOperandToken, InstructionIndex);
  } else if (isa<Argument>(Operand)) {
    return Token(TokenType::UnknownOperandToken, InstructionIndex);
  } else {
    return Token(TokenType::UnknownOperandToken, InstructionIndex);
  }
}

Token processOpcode(Instruction &IRInstruction, size_t InstructionIndex) {
  return Token(TokenType::OpcodeToken, InstructionIndex,
               IRInstruction.getOpcode());
}

std::vector<Token> processFunction(Function &IRFunction) {
  std::vector<Token> FunctionTokens;
  std::unordered_map<Value *, size_t> InstructionIndexMapping;
  size_t InstructionIndex = 0;
  for (BasicBlock &IRBB : IRFunction) {
    for (Instruction &IRInstruction : IRBB) {
      FunctionTokens.push_back(processOpcode(IRInstruction, InstructionIndex));
      InstructionIndexMapping[&IRInstruction] = InstructionIndex;
      for (unsigned i = 0; i < IRInstruction.getNumOperands(); ++i) {
        Value *Operand = IRInstruction.getOperand(i);
        FunctionTokens.push_back(
            processOperand(Operand, InstructionIndex, InstructionIndexMapping));
      }
      ++InstructionIndex;
    }
  }
  return FunctionTokens;
}

int main(int argc, char **argv) {
  cl::ParseCommandLineOptions(argc, argv, "llvm-tokenizer\n");

  LLVMContext Context;

  std::unique_ptr<llvm::MemoryBuffer> FileBuffer =
      ExitOnErr(errorOrToExpected(MemoryBuffer::getFile(InputFilename)));

  SMDiagnostic ParseError;
  std::unique_ptr<llvm::Module> IRModule =
      parseIR(*FileBuffer, ParseError, Context);

  if (!IRModule) {
    ParseError.print("Failed to parse: ", errs());
    return 1;
  }

  std::unique_ptr<ScopedPrinter> Writer =
      std::make_unique<ScopedPrinter>(fouts());

  for (Function &IRFunction : *IRModule) {
    Writer->objectBegin("function");
    Writer->printString("name", IRFunction.getName());
    Writer->arrayBegin("tokens");
    std::vector<Token> FunctionTokens = processFunction(IRFunction);
    for (Token SingleToken : FunctionTokens) {
      Writer->objectBegin();
      Writer->printString("type", GetTokenTypeName(SingleToken.Type));
      Writer->printNumber("instruction_index", SingleToken.InstructionIndex);
      if (SingleToken.Type == TokenType::OpcodeToken) {
        Writer->printNumber("opcode", SingleToken.Data.Opcode);
      } else if (SingleToken.Type == TokenType::InstructionOperandToken) {
        Writer->printNumber("instruction_reference",
                            SingleToken.Data.ReferencedInstructionIndex);
      }
      Writer->objectEnd();
    }
    Writer->arrayEnd();
    Writer->objectEnd();
  }

  return 0;
}
