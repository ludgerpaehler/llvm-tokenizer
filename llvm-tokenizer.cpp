#include "llvm/Support/CommandLine.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>

#include <iostream>

using namespace llvm;

static cl::opt<std::string> InputFilename(cl::Positional,
                                          cl::desc("Input Bitcode/Textual IR"),
                                          cl::init("-"));

static ExitOnError ExitOnErr("llvm-tokenizer error: ");

void processOperand(Value *Operand) {
  if (const Instruction *I = dyn_cast<Instruction>(Operand)) {
    std::cout << "operand:instruction:masked ";
  } else if (auto *ConstantOperand = llvm::dyn_cast<llvm::Constant>(Operand)) {
    std::cout << "operand:constant:masked ";
  } else if (const BasicBlock *BB = dyn_cast<BasicBlock>(Operand)) {
    std::cout << "operand:basicblock:masked ";
  } else if (const GlobalValue *GV = dyn_cast<GlobalValue>(Operand)) {
    std::cout << "operand:globalvalue:masked ";
  } else if (const MetadataAsValue *V = dyn_cast<MetadataAsValue>(Operand)) {
    std::cout << "operand:metadataasvalue:masked ";
  } else if (isa<InlineAsm>(Operand)) {
    std::cout << "operand:inlineasm:masked ";
  } else if (isa<Argument>(Operand)) {
    std::cout << "operand:argument:masked ";
  } else {
    std::cout << "operand:unknown ";
  }
}

void processFunction(Function &IRFunction) {
  for (BasicBlock &IRBB : IRFunction) {
    for (Instruction &IRInstruction : IRBB) {
      std::cout << IRInstruction.getOpcodeName() << " ";
      for (unsigned i = 0; i < IRInstruction.getNumOperands(); ++i) {
        Value *Operand = IRInstruction.getOperand(i);
        processOperand(Operand);
      }
      std::cout << "\n";
    }
  }
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
    processFunction(IRFunction);
  }

  return 0;
}
