#include <llvm/ADT/APFloat.h>
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

static constexpr const uint32_t OpcodeCount = 67;
static constexpr const uint32_t TypeCount = 22;
static constexpr const uint32_t AttributeCount = 200;

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

static cl::opt<std::string> IntConstantsListFilename(
    "int-constants-list",
    cl::desc("A list of integer constants that should be tokenized."),
    cl::init(""));

static cl::opt<uint32_t> MaxInstructionOperandReferenceDiff(
    "max-instruction-operand-reference-diff",
    cl::desc("The maximum numerical difference in instruction indexes to "
             "tokenize uniquely."),
    cl::init(32));

static cl::opt<bool> PrintSerializationConfig(
    "print-serialization-config",
    cl::desc(
        "Whether or not to print information on how the input was tokenized."),
    cl::init(false));

static ExitOnError ExitOnErr("llvm-tokenizer error: ");

enum TokenType {
  PaddingToken,
  InstructionOperandToken,
  ConstantIntegerOperandToken,
  ConstantFloatOperandToken,
  ConstantGlobalValueOperandToken,
  UnknownConstantOperandToken,
  BasicBlockOperandToken,
  InlineASMOperandToken,
  ArgumentOperandToken,
  UnknownOperandToken,
  OpcodeToken,
  TypeToken,
  AttributeToken,
  FunctionStartToken,
  FunctionEndToken
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
  case TokenType::ConstantGlobalValueOperandToken:
    return "global_value_operand";
  case TokenType::UnknownConstantOperandToken:
    return "unknown_constant_operand";
  case TokenType::BasicBlockOperandToken:
    return "basic_block_operand";
  case TokenType::InlineASMOperandToken:
    return "inline_ASM_operand";
  case TokenType::ArgumentOperandToken:
    return "argument_operand";
  case TokenType::UnknownOperandToken:
    return "unknown_operand";
  case TokenType::OpcodeToken:
    return "opcode";
  case TokenType::TypeToken:
    return "type";
  case TokenType::AttributeToken:
    return "attribute";
  case TokenType::FunctionStartToken:
    return "function_start";
  case TokenType::FunctionEndToken:
    return "function_end";
  }
  return "unknown_token";
}

union TokenData {
  unsigned Opcode;
  size_t ReferencedInstructionIndex;
  uint64_t ConstantIntegerValue;
  double ConstantFloatValue;
  uint8_t TypeID;
  unsigned AttributeID;
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
      if (&ConstantFloat->getValue().getSemantics() != &APFloat::IEEEquad() &&
          &ConstantFloat->getValue().getSemantics() !=
              &APFloat::x87DoubleExtended() &&
          &ConstantFloat->getValue().getSemantics() !=
              &APFloat::PPCDoubleDouble())
        ConstantOperandToken.Data.ConstantFloatValue =
            ConstantFloat->getValue().convertToDouble();
    } else if (const GlobalValue *GV = dyn_cast<GlobalValue>(Operand)) {
      ConstantOperandToken.Type = TokenType::ConstantGlobalValueOperandToken;
    }
    return ConstantOperandToken;
  } else if (const BasicBlock *BB = dyn_cast<BasicBlock>(Operand)) {
    return Token(TokenType::BasicBlockOperandToken, InstructionIndex);
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

  FunctionTokens.push_back(Token(TokenType::FunctionStartToken, 0));

  for (const auto &AttrSet : IRFunction.getAttributes()) {
    for (const auto &Attr : AttrSet) {
      if (Attr.isEnumAttribute() || Attr.isIntAttribute() ||
          Attr.isTypeAttribute()) {
        Token AttributeToken =
            Token(TokenType::AttributeToken, InstructionIndex);
        AttributeToken.Data.AttributeID =
            static_cast<unsigned>(Attr.getKindAsEnum());
        FunctionTokens.push_back(AttributeToken);
      }
    }
  }

  for (BasicBlock &IRBB : IRFunction) {
    for (Instruction &IRInstruction : IRBB) {
      FunctionTokens.push_back(processOpcode(IRInstruction, InstructionIndex));
      // Currently, only tokenize the type of the instruction.
      Token TypeToken(TokenType::TypeToken, InstructionIndex);
      TypeToken.Data.TypeID = IRInstruction.getType()->getTypeID();
      FunctionTokens.push_back(TypeToken);
      InstructionIndexMapping[&IRInstruction] = InstructionIndex;
      for (unsigned i = 0; i < IRInstruction.getNumOperands(); ++i) {
        Value *Operand = IRInstruction.getOperand(i);
        FunctionTokens.push_back(
            processOperand(Operand, InstructionIndex, InstructionIndexMapping));
      }
      ++InstructionIndex;
    }
  }

  FunctionTokens.push_back(Token(TokenType::FunctionEndToken, 0));

  return FunctionTokens;
}

std::unique_ptr<ScopedPrinter> WriterFactory() {
  if (TokenizerOutputMode == TokenizerOutputModeE::JSON)
    return std::make_unique<JSONScopedPrinter>(fouts(), PrettyPrintJSON);
  return std::make_unique<ScopedPrinter>(fouts());
}

std::unordered_map<int64_t, uint32_t>
LoadIntegerConstantsFromFile(StringRef FileName) {
  std::unordered_map<int64_t, uint32_t> IntegerConstants;
  ErrorOr<std::unique_ptr<MemoryBuffer>> FileBufferOrErr =
      MemoryBuffer::getFile(FileName);
  // TODO(boomanaiden154): Better error handling here when we can't load the
  // file for whatever reason.
  if (!FileBufferOrErr) {
    errs() << "Could not load integer constants file\n";
    return IntegerConstants;
  }

  SmallVector<StringRef> IntegerConstantStrings;
  FileBufferOrErr->get()->getBuffer().split(IntegerConstantStrings, '\n', -1,
                                            false);
  for (uint32_t i = 0; i < IntegerConstantStrings.size(); ++i) {
    IntegerConstants[std::stol(IntegerConstantStrings[i].data())] = i;
  }

  return IntegerConstants;
}

struct SerializationConfig {
  uint32_t PaddingTokenIndex;
  uint32_t InstructionOperandIndex;
  uint32_t ConstantIntegerOperandIndex;
  uint32_t ConstantFloatOperandIndex;
  uint32_t ConstantGlobalValueOperandIndex;
  uint32_t UnknownConstantOperandIndex;
  uint32_t BasicBlockOperandIndex;
  uint32_t InlineASMOperandIndex;
  uint32_t ArgumentOperandIndex;
  uint32_t UnknownOperandIndex;
  uint32_t OpcodeIndex;
  uint32_t TypeIndex;
  uint32_t AttributeIndex;
  uint32_t FunctionStartIndex;
  uint32_t FunctionEndIndex;

  SerializationConfig(uint32_t InstructionOperandSize,
                      uint32_t ConstantIntegerOperandSize) {
    PaddingTokenIndex = 0;
    InstructionOperandIndex = PaddingTokenIndex + 1;
    ConstantIntegerOperandIndex =
        InstructionOperandIndex + InstructionOperandSize;
    ConstantFloatOperandIndex =
        ConstantIntegerOperandIndex + ConstantIntegerOperandSize;
    ConstantGlobalValueOperandIndex = ConstantFloatOperandIndex + 1;
    UnknownConstantOperandIndex = ConstantGlobalValueOperandIndex + 1;
    BasicBlockOperandIndex = UnknownConstantOperandIndex + 1;
    InlineASMOperandIndex = BasicBlockOperandIndex + 1;
    ArgumentOperandIndex = InlineASMOperandIndex + 1;
    UnknownOperandIndex = ArgumentOperandIndex + 1;
    OpcodeIndex = UnknownOperandIndex + 1;
    TypeIndex = OpcodeIndex + OpcodeCount + 1;
    AttributeIndex = TypeIndex + TypeCount + 1;
    FunctionStartIndex = AttributeIndex + AttributeCount + 1;
    FunctionEndIndex = FunctionStartIndex + 1;
  }
};

void printSerConfig(const SerializationConfig &SerConfig,
                    ScopedPrinter &Writer) {
  Writer.objectBegin("config");
  Writer.printNumber("PaddingTokenIndex", SerConfig.PaddingTokenIndex);
  Writer.objectBegin("InstructionOperandRange");
  Writer.printNumber("Begin", SerConfig.InstructionOperandIndex);
  Writer.printNumber("End", SerConfig.ConstantIntegerOperandIndex - 1);
  Writer.objectEnd();
  Writer.objectBegin("ConstantOperandRange");
  Writer.printNumber("Begin", SerConfig.ConstantIntegerOperandIndex);
  Writer.printNumber("End", SerConfig.ConstantFloatOperandIndex - 1);
  Writer.objectEnd();
  Writer.printNumber("ConstantFloatOperandIndex",
                     SerConfig.ConstantFloatOperandIndex);
  Writer.printNumber("ConstantGlobalValueIndex",
                     SerConfig.ConstantGlobalValueOperandIndex);
  Writer.printNumber("UnknownConstantOperandIndex",
                     SerConfig.UnknownConstantOperandIndex);
  Writer.printNumber("BasicBlockOperandIndex",
                     SerConfig.BasicBlockOperandIndex);
  Writer.printNumber("InlineASMOperandIndex", SerConfig.InlineASMOperandIndex);
  Writer.printNumber("ArgumentOperandIndex", SerConfig.ArgumentOperandIndex);
  Writer.printNumber("UnknownOperandIndex", SerConfig.UnknownOperandIndex);
  Writer.objectBegin("OpcodeRange");
  Writer.printNumber("Begin", SerConfig.OpcodeIndex);
  Writer.printNumber("End", SerConfig.OpcodeIndex + OpcodeCount);
  Writer.objectEnd();
  Writer.objectBegin("TypeRange");
  Writer.printNumber("Begin", SerConfig.TypeIndex);
  Writer.printNumber("End", SerConfig.TypeIndex + TypeCount);
  Writer.objectEnd();
  Writer.objectBegin("AttributeRange");
  Writer.printNumber("Begin", SerConfig.AttributeIndex);
  Writer.printNumber("End", SerConfig.AttributeIndex + AttributeCount);
  Writer.objectEnd();
  Writer.printNumber("FunctionStart", SerConfig.FunctionStartIndex);
  Writer.printNumber("FunctionEnd", SerConfig.FunctionEndIndex);
  Writer.objectEnd();
}

void printTokenizedFunction(const std::string &FunctionName,
                            const std::vector<Token> &FunctionTokens,
                            ScopedPrinter &Writer) {
  Writer.objectBegin();
  Writer.printString("name", FunctionName);
  Writer.arrayBegin("tokens");
  for (Token SingleToken : FunctionTokens) {
    Writer.objectBegin();
    Writer.printString("type", GetTokenTypeName(SingleToken.Type));

    if (SingleToken.Type == TokenType::FunctionStartToken ||
        SingleToken.Type == TokenType::FunctionEndToken) {
      Writer.objectEnd();
      continue;
    }

    Writer.printNumber("instruction_index", SingleToken.InstructionIndex);
    if (SingleToken.Type == TokenType::OpcodeToken) {
      Writer.printNumber("opcode", SingleToken.Data.Opcode);
    } else if (SingleToken.Type == TokenType::InstructionOperandToken) {
      Writer.printNumber("instruction_reference",
                         SingleToken.Data.ReferencedInstructionIndex);
    } else if (SingleToken.Type == TokenType::ConstantIntegerOperandToken) {
      Writer.printNumber("integer_constant",
                         SingleToken.Data.ConstantIntegerValue);
    } else if (SingleToken.Type == TokenType::ConstantFloatOperandToken) {
      Writer.printNumber("float_constant",
                         (double)SingleToken.Data.ConstantFloatValue);
    } else if (SingleToken.Type == TokenType::TypeToken) {
      Writer.printNumber("type_id", SingleToken.Data.TypeID);
    } else if (SingleToken.Type == TokenType::AttributeToken) {
      Writer.printNumber("attribute_id", SingleToken.Data.AttributeID);
    }
    Writer.objectEnd();
  }
  Writer.arrayEnd();
  Writer.objectEnd();
}

std::vector<uint32_t> SerializeFunctionFromTokens(
    const std::vector<Token> &FunctionTokens,
    const std::unordered_map<int64_t, uint32_t> IntegerConstantsToTokenize,
    const SerializationConfig &SerConfig) {
  std::vector<uint32_t> SerializedTokens;
  SerializedTokens.reserve(FunctionTokens.size());

  for (const Token &SingleToken : FunctionTokens) {
    if (SingleToken.Type == TokenType::PaddingToken) {
      SerializedTokens.push_back(SerConfig.PaddingTokenIndex);
    } else if (SingleToken.Type == TokenType::InstructionOperandToken) {
      // Get the distance from the current instruction to the instruction
      // thati is being referred to. This number will always be positive as
      // we're in SSA.
      int32_t InstructionDistance = SingleToken.InstructionIndex -
                                    SingleToken.Data.ReferencedInstructionIndex;
      assert(InstructionDistance > 0);
      if (InstructionDistance > MaxInstructionOperandReferenceDiff) {
        InstructionDistance = MaxInstructionOperandReferenceDiff;
      }
      SerializedTokens.push_back(SerConfig.InstructionOperandIndex +
                                 InstructionDistance);
    } else if (SingleToken.Type == TokenType::ConstantIntegerOperandToken) {
      if (IntegerConstantsToTokenize.count(
              SingleToken.Data.ConstantIntegerValue) != 0) {
        SerializedTokens.push_back(SerConfig.ConstantIntegerOperandIndex +
                                   IntegerConstantsToTokenize.at(
                                       SingleToken.Data.ConstantIntegerValue));
      } else {
        SerializedTokens.push_back(SerConfig.ConstantIntegerOperandIndex +
                                   IntegerConstantsToTokenize.size());
      }
    } else if (SingleToken.Type == TokenType::ConstantFloatOperandToken) {
      SerializedTokens.push_back(SerConfig.ConstantFloatOperandIndex);
    } else if (SingleToken.Type == TokenType::ConstantGlobalValueOperandToken) {
      SerializedTokens.push_back(SerConfig.ConstantGlobalValueOperandIndex);
    } else if (SingleToken.Type == TokenType::UnknownConstantOperandToken) {
      SerializedTokens.push_back(SerConfig.UnknownConstantOperandIndex);
    } else if (SingleToken.Type == TokenType::BasicBlockOperandToken) {
      SerializedTokens.push_back(SerConfig.BasicBlockOperandIndex);
    } else if (SingleToken.Type == TokenType::InlineASMOperandToken) {
      SerializedTokens.push_back(SerConfig.InlineASMOperandIndex);
    } else if (SingleToken.Type == TokenType::ArgumentOperandToken) {
      SerializedTokens.push_back(SerConfig.ArgumentOperandIndex);
    } else if (SingleToken.Type == TokenType::UnknownOperandToken) {
      SerializedTokens.push_back(SerConfig.UnknownOperandIndex);
    } else if (SingleToken.Type == TokenType::OpcodeToken) {
      SerializedTokens.push_back(SerConfig.OpcodeIndex +
                                 SingleToken.Data.Opcode);
    } else if (SingleToken.Type == TokenType::TypeToken) {
      SerializedTokens.push_back(SerConfig.TypeIndex + SingleToken.Data.TypeID);
    } else if (SingleToken.Type == TokenType::AttributeToken) {
      SerializedTokens.push_back(SerConfig.AttributeIndex +
                                 SingleToken.Data.AttributeID);
    } else if (SingleToken.Type == TokenType::FunctionStartToken) {
      SerializedTokens.push_back(SerConfig.FunctionStartIndex);
    } else if (SingleToken.Type == TokenType::FunctionEndToken) {
      SerializedTokens.push_back(SerConfig.FunctionEndIndex);
    }
  }

  return SerializedTokens;
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
    if (IRFunction.isDeclaration())
      continue;

    FunctionTokens[IRFunction.getName().str()] = processFunction(IRFunction);
  }

  std::unique_ptr<ScopedPrinter> Writer = WriterFactory();

  if (TokenizationMode == TokenizationModeE::Tokenize) {
    std::unique_ptr<ScopedPrinter> Writer = WriterFactory();
    Writer->arrayBegin("functions");
    for (auto TokenizedFunctionItr : FunctionTokens) {
      printTokenizedFunction(TokenizedFunctionItr.first,
                             TokenizedFunctionItr.second, *Writer);
    }
    Writer->arrayEnd();
    return 0;
  }

  // We're assuming we're in serialization mode.
  std::unordered_map<int64_t, uint32_t> IntegerConstantsToTokenize =
      LoadIntegerConstantsFromFile(IntConstantsListFilename);

  // Add one to each of the sizes as we also need to include the "out of range"
  // tokens that are the last index in the range.
  SerializationConfig SerConfig(MaxInstructionOperandReferenceDiff + 1,
                                IntegerConstantsToTokenize.size() + 1);

  if (PrintSerializationConfig) {
    printSerConfig(SerConfig, *Writer);
  }

  Writer->arrayBegin("functions");
  for (auto TokenizedFunctionItr : FunctionTokens) {
    Writer->objectBegin();
    Writer->printString("name", TokenizedFunctionItr.first);
    Writer->printList("tokens", SerializeFunctionFromTokens(
                                    TokenizedFunctionItr.second,
                                    IntegerConstantsToTokenize, SerConfig));
    Writer->objectEnd();
  }
  Writer->arrayEnd();

  return 0;
}
