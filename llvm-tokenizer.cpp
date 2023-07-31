#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/SourceMgr.h>

#include <iostream>

using namespace llvm;

static ExitOnError ExitOnErr("llvm-tokenizer error: ");

int main() {
  LLVMContext Context;

  std::unique_ptr<llvm::MemoryBuffer> FileBuffer = ExitOnErr(errorOrToExpected(MemoryBuffer::getFile("./test.ll")));

  SMDiagnostic ParseError;
  std::unique_ptr<llvm::Module> IRModule = parseIR(*FileBuffer, ParseError, Context);

  if(!IRModule) {
    ParseError.print("./test.ll", errs());
    return 1;
  }

  for(Function &IRFunction : *IRModule) {
    std::cout << IRFunction.getName().str() << "\n";
  }

  std::cout << "Hello World\n";
  return 0;
}
