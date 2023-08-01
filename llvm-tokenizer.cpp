#include "llvm/Support/CommandLine.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>

#include <iostream>
#include <vector>

using namespace llvm;

static cl::opt<std::string> InputFilename(cl::Positional,
                                          cl::desc("Input Bitcode/Textual IR"),
                                          cl::init("-"));

static ExitOnError ExitOnErr("llvm-tokenizer error: ");

using TokenType = unsigned;

enum class ReservedTokens : TokenType {
  // Set the first token to one, so the zeroth token can be user-defined.
  PaddingToken = 1,
  InstructionOperandToken,
  ConstantOperandToken,
  BasicBlockOperandToken,
  GlobalValueOperandToken,
  MetadataAsValueOperandToken,
  InlineASMOperandToken,
  ArgumentOperandToken,
  UnknownOperandToken,
  OpcodeOffset
};

struct Token {
  TokenType Type;

  Token(TokenType _Type) : Type(_Type) {}
};

TokenType GetReservedToken(ReservedTokens ReservedToken) {
  return static_cast<TokenType>(ReservedToken);
}

Token processOperand(Value *Operand) {
  if (const Instruction *I = dyn_cast<Instruction>(Operand)) {
    return Token(GetReservedToken(ReservedTokens::InstructionOperandToken));
  } else if (auto *ConstantOperand = llvm::dyn_cast<llvm::Constant>(Operand)) {
    return Token(GetReservedToken(ReservedTokens::ConstantOperandToken));
  } else if (const BasicBlock *BB = dyn_cast<BasicBlock>(Operand)) {
    return Token(GetReservedToken(ReservedTokens::BasicBlockOperandToken));
  } else if (const GlobalValue *GV = dyn_cast<GlobalValue>(Operand)) {
    return Token(GetReservedToken(ReservedTokens::GlobalValueOperandToken));
  } else if (const MetadataAsValue *V = dyn_cast<MetadataAsValue>(Operand)) {
    return Token(GetReservedToken(ReservedTokens::MetadataAsValueOperandToken));
  } else if (isa<InlineAsm>(Operand)) {
    return Token(GetReservedToken(ReservedTokens::InlineASMOperandToken));
  } else if (isa<Argument>(Operand)) {
    return Token(GetReservedToken(ReservedTokens::UnknownOperandToken));
  } else {
    return Token(GetReservedToken(ReservedTokens::UnknownOperandToken));
  }
}

Token processOpcode(Instruction &IRInstruction) {
  return Token(GetReservedToken(ReservedTokens::OpcodeOffset) +
               IRInstruction.getOpcode());
}

std::vector<Token> processFunction(Function &IRFunction) {
  std::vector<Token> FunctionTokens;
  for (BasicBlock &IRBB : IRFunction) {
    for (Instruction &IRInstruction : IRBB) {
      FunctionTokens.push_back(processOpcode(IRInstruction));
      for (unsigned i = 0; i < IRInstruction.getNumOperands(); ++i) {
        Value *Operand = IRInstruction.getOperand(i);
        FunctionTokens.push_back(processOperand(Operand));
      }
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

  for (Function &IRFunction : *IRModule) {
    std::cout << "*** Starting new function: " << IRFunction.getName().data()
              << " ***\n";
    std::vector<Token> FunctionTokens = processFunction(IRFunction);
    for (Token SingleToken : FunctionTokens) {
      std::cout << static_cast<unsigned>(SingleToken.Type) << "\n";
    }
  }

  return 0;
}
