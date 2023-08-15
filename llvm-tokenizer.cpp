#include <llvm/IR/Constants.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/ScopedPrinter.h>
#include <llvm/Support/SourceMgr.h>

#include <memory>
#include <unordered_map>
#include <vector>

using namespace llvm;

enum TokenizerOutputModeE { JSON, Standard };

enum TokenizationModeE { Tokenize, Serialize };

static cl::opt<std::string> InputFilename(cl::Positional,
                                          cl::desc("Input Bitcode/Textual IR"),
                                          cl::init("-"));

static cl::opt<TokenizerOutputModeE> TokenizerOutputMode(
    "output-mode", cl::desc("The type of output to produce."),
    cl::values(clEnumValN(TokenizerOutputModeE::Standard, "standard",
                          "The standard output mode."),
               clEnumValN(TokenizerOutputModeE::JSON, "json", "JSON output.")),
    cl::init(TokenizerOutputModeE::Standard));

static cl::opt<bool>
    PrettyPrintJSON("pretty-print",
                    cl::desc("Whether or not to pretty print JSON output."),
                    cl::init(false));

static cl::opt<TokenizationModeE> TokenizationMode(
    "mode", cl::desc("The mode to run llvm-tokenizer in."),
    cl::values(clEnumValN(TokenizationModeE::Tokenize, "tokenize",
                          "Tokenize the input and output raw tokens"),
               clEnumValN(TokenizationModeE::Serialize, "serialize",
                          "Tokenize the input and output a stream of integers "
                          "representing serialized tokens.")),
    cl::init(TokenizationModeE::Tokenize));

static ExitOnError ExitOnErr("llvm-tokenizer error: ");

enum TokenType {
  PaddingToken,
  InstructionOperandToken,
  ConstantIntegerOperandToken,
  ConstantFloatOperandToken,
  UnknownConstantOperandToken,
  BasicBlockOperandToken,
  GlobalValueOperandToken,
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
  case TokenType::ConstantIntegerOperandToken:
    return "constant_integer_operand";
  case TokenType::ConstantFloatOperandToken:
    return "constant_float_operand";
  case TokenType::UnknownConstantOperandToken:
    return "unknown_constant_operand";
  case TokenType::BasicBlockOperandToken:
    return "basic_block_operand";
  case TokenType::GlobalValueOperandToken:
    return "global_value_operand";
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
  uint64_t ConstantIntegerValue;
  double ConstantFloatValue;
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
    if (InstructionIndexMappingItr == InstructionIndexMapping.end()) {
      // TODO(boomanaiden154): See how often this case occurs and find out a
      // better way to deal with it.
      InstructionOperandToken.Data.ReferencedInstructionIndex = 0;
    } else {
      InstructionOperandToken.Data.ReferencedInstructionIndex =
          InstructionIndexMappingItr->second;
    }
    return InstructionOperandToken;
    return Token(TokenType::InstructionOperandToken, InstructionIndex);
  } else if (const Constant *ConstantOperand = dyn_cast<Constant>(Operand)) {
    Token ConstantOperandToken =
        Token(TokenType::UnknownConstantOperandToken, InstructionIndex);
    if (const ConstantInt *ConstantInteger =
            dyn_cast<ConstantInt>(ConstantOperand)) {
      ConstantOperandToken.Type = TokenType::ConstantIntegerOperandToken;
      ConstantOperandToken.Data.ConstantIntegerValue =
          ConstantInteger->getValue().getLimitedValue();
    } else if (const ConstantFP *ConstantFloat =
                   dyn_cast<ConstantFP>(ConstantOperand)) {
      ConstantOperandToken.Type = TokenType::ConstantFloatOperandToken;
      ConstantOperandToken.Data.ConstantFloatValue =
          ConstantFloat->getValue().convertToDouble();
    }
    return ConstantOperandToken;
  } else if (const BasicBlock *BB = dyn_cast<BasicBlock>(Operand)) {
    return Token(TokenType::BasicBlockOperandToken, InstructionIndex);
  } else if (const GlobalValue *GV = dyn_cast<GlobalValue>(Operand)) {
    return Token(TokenType::GlobalValueOperandToken, InstructionIndex);
  } else if (isa<InlineAsm>(Operand)) {
    return Token(TokenType::InlineASMOperandToken, InstructionIndex);
  } else if (isa<Argument>(Operand)) {
    return Token(TokenType::ArgumentOperandToken, InstructionIndex);
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

std::unique_ptr<ScopedPrinter> WriterFactory() {
  if (TokenizerOutputMode == TokenizerOutputModeE::JSON)
    return std::make_unique<JSONScopedPrinter>(fouts(), PrettyPrintJSON);
  return std::make_unique<ScopedPrinter>(fouts());
}

int main(int argc, char **argv) {
  cl::ParseCommandLineOptions(argc, argv, "llvm-tokenizer\n");

  LLVMContext Context;

  SMDiagnostic ParseError;
  std::unique_ptr<llvm::Module> IRModule =
      parseIRFile(InputFilename, ParseError, Context);

  if (!IRModule) {
    ParseError.print("Failed to parse: ", errs());
    return 1;
  }

  std::unordered_map<std::string, std::vector<Token>> FunctionTokens;

  for (Function &IRFunction : *IRModule) {
    FunctionTokens[IRFunction.getName().str()] = processFunction(IRFunction);
  }

  std::unique_ptr<ScopedPrinter> Writer = WriterFactory();

  if (TokenizationMode == TokenizationModeE::Tokenize) {
    std::unique_ptr<ScopedPrinter> Writer = WriterFactory();
    Writer->arrayBegin();
    for (auto TokenizedFunctionItr : FunctionTokens) {
      Writer->objectBegin();
      Writer->printString("name", TokenizedFunctionItr.first);
      ;
      Writer->arrayBegin("tokens");
      for (Token SingleToken : TokenizedFunctionItr.second) {
        Writer->objectBegin();
        Writer->printString("type", GetTokenTypeName(SingleToken.Type));
        Writer->printNumber("instruction_index", SingleToken.InstructionIndex);
        if (SingleToken.Type == TokenType::OpcodeToken) {
          Writer->printNumber("opcode", SingleToken.Data.Opcode);
        } else if (SingleToken.Type == TokenType::InstructionOperandToken) {
          Writer->printNumber("instruction_reference",
                              SingleToken.Data.ReferencedInstructionIndex);
        } else if (SingleToken.Type == TokenType::ConstantIntegerOperandToken) {
          Writer->printNumber("integer_constant",
                              SingleToken.Data.ConstantIntegerValue);
        } else if (SingleToken.Type == TokenType::ConstantFloatOperandToken) {
          Writer->printNumber("float_constant",
                              (double)SingleToken.Data.ConstantFloatValue);
        }
        Writer->objectEnd();
      }
      Writer->arrayEnd();
      Writer->objectEnd();
    }
    Writer->arrayEnd();
    return 0;
  }

  // We're assuming we're in serialization mode.

  Writer->arrayBegin();
  for (auto TokenizedFunctionItr : FunctionTokens) {
    Writer->objectBegin();
    Writer->printString("name", TokenizedFunctionItr.first);

    std::vector<uint32_t> SerializedTokens;
    SerializedTokens.reserve(TokenizedFunctionItr.second.size());

    // TODO(boomanaiden154): Make these constants configurable.

    uint32_t ConstantIntegerOperandSize = 1002;
    uint32_t InstructionOperandReferenceSize = 32;

    // TODO(boomanaiden154): Figure out a more elegant way of constructing this.

    uint32_t PaddingTokenIndex = 0;
    uint32_t InstructionOperandIndex = 1;
    uint32_t ConstantIntegerOperandIndex =
        InstructionOperandIndex + InstructionOperandReferenceSize;
    uint32_t ConstantFloatOperandIndex =
        ConstantIntegerOperandIndex + ConstantIntegerOperandSize;
    uint32_t UnknownConstantOperandTokenIndex = ConstantFloatOperandIndex + 1;
    uint32_t BasicBlockOperandTokenIndex = UnknownConstantOperandTokenIndex + 1;
    uint32_t GlobalValueOperandTokenIndex = BasicBlockOperandTokenIndex + 1;
    uint32_t InlineASMOperandTokenIndex = GlobalValueOperandTokenIndex + 1;
    uint32_t ArgumentOperandTokenIndex = InlineASMOperandTokenIndex + 1;
    uint32_t UnknownOperandTokenIndex = ArgumentOperandTokenIndex + 1;
    uint32_t OpcodeTokenIndex = UnknownOperandTokenIndex + 1;

    for (Token &SingleToken : TokenizedFunctionItr.second) {
      if (SingleToken.Type == TokenType::PaddingToken) {
        SerializedTokens.push_back(PaddingTokenIndex);
      } else if (SingleToken.Type == TokenType::InstructionOperandToken) {
        // Get the distance from the current instruction to the instruction that
        // is being referred to. This number will always be positive as we're in
        // SSA.
        uint32_t InstructionDistance =
            SingleToken.InstructionIndex -
            SingleToken.Data.ReferencedInstructionIndex;
        if (InstructionDistance > InstructionOperandReferenceSize) {
          InstructionDistance = InstructionOperandReferenceSize - 1;
        }
        SerializedTokens.push_back(InstructionOperandIndex +
                                   InstructionDistance);
      } else if (SingleToken.Type == TokenType::ConstantIntegerOperandToken) {
        // TODO(boomanaiden154): Make this actually read from a file rather than
        // just looking at constants below 1000.
        if (SingleToken.Data.ConstantIntegerValue < 1000)
          SerializedTokens.push_back(ConstantIntegerOperandIndex +
                                     SingleToken.Data.ConstantIntegerValue);
        else
          SerializedTokens.push_back(ConstantIntegerOperandSize - 1);
      } else if (SingleToken.Type == TokenType::ConstantFloatOperandToken) {
        SerializedTokens.push_back(ConstantFloatOperandIndex);
      } else if (SingleToken.Type == TokenType::UnknownConstantOperandToken) {
        SerializedTokens.push_back(UnknownConstantOperandTokenIndex);
      } else if (SingleToken.Type == TokenType::BasicBlockOperandToken) {
        SerializedTokens.push_back(BasicBlockOperandTokenIndex);
      } else if (SingleToken.Type == TokenType::GlobalValueOperandToken) {
        SerializedTokens.push_back(GlobalValueOperandTokenIndex);
      } else if (SingleToken.Type == TokenType::InlineASMOperandToken) {
        SerializedTokens.push_back(InlineASMOperandTokenIndex);
      } else if (SingleToken.Type == TokenType::ArgumentOperandToken) {
        SerializedTokens.push_back(ArgumentOperandTokenIndex);
      } else if (SingleToken.Type == TokenType::UnknownOperandToken) {
        SerializedTokens.push_back(UnknownOperandTokenIndex);
      } else if (SingleToken.Type == TokenType::OpcodeToken) {
        SerializedTokens.push_back(OpcodeTokenIndex + SingleToken.Data.Opcode);
      }
    }

    Writer->printList("tokens", SerializedTokens);
    Writer->objectEnd();
  }
  Writer->arrayEnd();

  return 0;
}
