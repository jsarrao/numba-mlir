// SPDX-FileCopyrightText: 2022 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "numba/ExecutionEngine/ExecutionEngine.hpp"

#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/GraphTraits.h>
#include <llvm/ADT/SetVector.h>
#include <llvm/ADT/iterator.h>
#include <llvm/ADT/iterator_range.h>
#include <llvm/Analysis/AssumptionCache.h>
#include <llvm/Analysis/LoopNestAnalysis.h>
#include <llvm/Analysis/LoopPass.h>
#include <llvm/Analysis/MemorySSAUpdater.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/Analysis/TargetTransformInfo.h>
#include <llvm/ExecutionEngine/JITEventListener.h>
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/ObjectCache.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>
#include <llvm/ExecutionEngine/Orc/IRTransformLayer.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/Orc/Shared/ExecutorAddress.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/User.h>
#include <llvm/IR/Value.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Pass.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/TapirUtils.h>
#include <llvm/Transforms/Utils/ValueMapper.h>

#include <mlir/IR/BuiltinOps.h>
#include <mlir/Support/FileUtilities.h>
#include <mlir/Target/LLVMIR/Export.h>

#include <llvm/IR/PassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/Passes/StandardInstrumentations.h>

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <iterator>

#define DEBUG_TYPE "numba-execution-engine"

static llvm::OptimizationLevel mapToLevel(llvm::CodeGenOptLevel level) {
  unsigned optimizeSize = 0; // TODO: unhardcode

  switch (level) {
  default:
    llvm_unreachable("Invalid optimization level!");

  case llvm::CodeGenOptLevel::None:
    return llvm::OptimizationLevel::O0;

  case llvm::CodeGenOptLevel::Less:
    return llvm::OptimizationLevel::O1;

  case llvm::CodeGenOptLevel::Default:
    switch (optimizeSize) {
    default:
      llvm_unreachable("Invalid optimization level for size!");

    case 0:
      return llvm::OptimizationLevel::O2;

    case 1:
      return llvm::OptimizationLevel::Os;

    case 2:
      return llvm::OptimizationLevel::Oz;
    }

  case llvm::CodeGenOptLevel::Aggressive:
    return llvm::OptimizationLevel::O3;
  }
}

static llvm::PipelineTuningOptions
getPipelineTuningOptions(llvm::CodeGenOptLevel optLevelVal) {
  llvm::PipelineTuningOptions pto;
  auto level = static_cast<int>(optLevelVal);

  pto.LoopUnrolling = level > 0;
  pto.LoopVectorization = level > 1;
  pto.SLPVectorization = level > 1;
  return pto;
}

namespace llvm {
struct tapirifyLoopPass : PassInfoMixin<tapirifyLoopPass> {
  bool splitLoop(Loop *l, Function &f, ScalarEvolution &se) {
    BasicBlock *header = l->getHeader();
    Module *m = f.getParent();
    LLVMContext &context = m->getContext();

    // create list of all phi nodes in the loops header block. one of these
    // should be the loop induction variable
    SmallVector<PHINode *> PHICandidates;
    for (PHINode &pn : header->phis()) {
      if (pn.getNumIncomingValues() == 2) {
        for (uint i = 0; i < 2; i++) {
          if (ConstantInt *CI = dyn_cast<ConstantInt>(pn.getIncomingValue(i))) {
            // if one of the incoming values is 0, the PHI is potentially the
            // loop induction variable
            if (CI->isZero()) {
              PHICandidates.push_back(&pn);
            }
          }
        }
      }
    }

    // look at the scaler evolution of each phi node add see if it is add rec
    // with a constant of one, which would mean it is a canonical induction
    // variable
    PHINode *canonInduct = nullptr;
    for (PHINode *pn : PHICandidates) {
      const SCEV *phiSCEV = se.getSCEV(pn);
      if (const SCEVAddRecExpr *ARSCEV = dyn_cast<SCEVAddRecExpr>(phiSCEV)) {
        const SCEV *stepSCEV = ARSCEV->getStepRecurrence(se);
        if (const SCEVConstant *constSCEV = dyn_cast<SCEVConstant>(stepSCEV)) {
          ConstantInt *stepVal = constSCEV->getValue();
          if (stepVal->isOne()) {
            canonInduct = pn;
          }
        }
      }
    }
    if (!canonInduct) {
      return false;
    }

    // if a canonical induction variable is found, it is fine to 'tapirify' and
    // the loops body can be sepearted from the loop into a tapir region. the
    // phi nodes and increment, compare, and branch instructions that define the
    // loop must be outside the tapir region

    // first the increment instruction is found
    BasicBlock *exit = l->getExitBlock();
    Instruction *increment = nullptr;
    for (Use &use : canonInduct->uses()) {
      auto *user = use.getUser();
      Instruction *userI = dyn_cast<Instruction>(user);
      if (userI->getOpcode() == Instruction::Add) {
        // check if either operand is a ConstantInt of value 1
        for (uint i = 0; i < 2; i++) {
          if (auto *opVal = dyn_cast<ConstantInt>(userI->getOperand(i))) {
            if (opVal->getSExtValue() == 1) {
              increment = userI;
            }
          }
        }
      }
    }
    if (!increment) {
      return false;
    }

    // find compare instruction that uses the increment instruction
    Instruction *icmp = nullptr;
    for (Use &use : increment->uses()) {
      if (auto *userI = dyn_cast<ICmpInst>(use.getUser())) {
        icmp = userI;
      }
    }
    if (!icmp) {
      return false;
    }

    // find branch instruction that uses compare instruction
    Instruction *branch = nullptr;
    for (Use &use : icmp->uses()) {
      if (auto *userI = dyn_cast<BranchInst>(use.getUser())) {
        branch = userI;
      }
    }
    if (!branch) {
      return false;
    }

    // now we need to split the blocks so that we can add in the tapir
    // instructions
    Instruction *firstSplitPoint;
    Instruction *secondSplitPoint;

    // if increment comes before loop body, split points end up being
    // slightly different
    if (increment->getNextNonDebugInstruction() == icmp) {
      firstSplitPoint = header->getFirstNonPHI();
      secondSplitPoint = increment;
    } else {
      firstSplitPoint = increment->getNextNonDebugInstruction();
      secondSplitPoint = icmp;
    }
    if (!firstSplitPoint || !secondSplitPoint) {
      return false;
    }

    // splitting at the first split point
    BasicBlock *parent1 = firstSplitPoint->getParent();
    BasicBlock *body = parent1->splitBasicBlock(firstSplitPoint, "body", false);
    if (body == nullptr) {
      return false;
    }

    auto *parent2 = secondSplitPoint->getParent();
    BasicBlock *latch =
        parent2->splitBasicBlock(secondSplitPoint, "latch", false);
    if (latch == nullptr) {
      return false;
    }

    // add in tapir instructions
    // creating sync region
    FunctionType *syncType =
        FunctionType::get(Type::getTokenTy(context), {}, false);
    FunctionCallee syncStart =
        m->getOrInsertFunction("llvm.syncregion.start", syncType);
    BasicBlock &entry = f.getEntryBlock();
    Instruction *insertPoint = entry.getFirstNonPHI();
    auto *syncRegInst = CallInst::Create(syncStart, {}, "syncreg", insertPoint);
    syncRegInst->setTailCall();

    // add detach to block that preceeds the first split point
    BasicBlock *detachBlock = parent1;
    Instruction *detachTerm = detachBlock->getTerminator();
    detachTerm->eraseFromParent();
    DetachInst::Create(body, latch, syncRegInst, detachBlock);

    // add in reattach to block that preceeds the second split point
    BasicBlock *latchPred = parent2;
    Instruction *bodyTerm = latchPred->getTerminator();
    bodyTerm->eraseFromParent();
    ReattachInst::Create(latch, syncRegInst, latchPred);

    // add sync inst to block that the latch exits to
    for (unsigned i = 0; i < branch->getNumOperands(); i++) {
      Value *op = branch->getOperand(i);
      if (isa<BasicBlock>(op) && op == exit) {
        BasicBlock *newExit = BasicBlock::Create(context, "newexit", &f);
        SyncInst::Create(exit, syncRegInst, newExit);
        branch->setOperand(i, newExit);
        exit->replacePhiUsesWith(branch->getParent(), newExit);
      }
    }

    // add neccessary tapir metadata to the loop
    auto *Int32Ty = Type::getInt32Ty(context);
    auto *branchMD = MDNode::getDistinct(context, {});
    auto *tapirSpawnStrat = MDNode::get(
        context, {MDString::get(context, "tapir.loop.spawn.strategy"),
                  ConstantAsMetadata::get(ConstantInt::get(
                      Int32Ty, TapirLoopHints::SpawningStrategy::ST_DAC))});
    MDNode *targetID;
    const char* tapirTarget = std::getenv("NM_TAPIRTARGET");
    if (strcmp(tapirTarget, "opencilk") == 0) {
      targetID = MDNode::get(context, {MDString::get(context, "tapir.loop.target"), ConstantAsMetadata::get(ConstantInt::get(Int32Ty, (uint64_t) TapirTargetID::OpenCilk))});
    } else {
      targetID = MDNode::get(context, {MDString::get(context, "tapir.loop.target"), ConstantAsMetadata::get(ConstantInt::get(Int32Ty, (uint64_t) TapirTargetID::Cuda))});
    }

    branchMD->push_back(branchMD);
    branchMD->push_back(tapirSpawnStrat);
    branchMD->push_back(targetID);

    branch->setMetadata("llvm.loop", branchMD);
    return true;
  }

  PreservedAnalyses run(Function &f, FunctionAnalysisManager &am) {
    auto &li = am.getResult<LoopAnalysis>(f);
    auto &se = am.getResult<ScalarEvolutionAnalysis>(f);
    for (auto l : li) {
      LoopNest ln(*l, se);
      int numNested = (int)ln.getNumLoops();
      for (int i = numNested - 1; i >= 0; i--) {
        auto *nestedLoop = ln.getLoop(i);
        if (splitLoop(nestedLoop, f, se)) {
          // if splitLoop returns true, one of the loops in the loop nest has
          // been tapirified, and we cannot tapirify any of the outer loops
          break;
        }
      }
    }
    return PreservedAnalyses::none();
  }

  static bool isRequired() { return true; }
};

struct replaceNRTAllocPass : PassInfoMixin<replaceNRTAllocPass> {
  PreservedAnalyses run(Function &f, FunctionAnalysisManager &am) {
    Module *m = f.getParent();
    SmallVector<CallInst *> replaceList;
    for (BasicBlock &bb : f) {
      for (Instruction &i : bb) {
        if (auto *ci = dyn_cast<CallInst>(&i)) {
          auto fname = ci->getCalledFunction()->getName();
          if (fname == "NRT_MemInfo_alloc_safe_aligned") {
            replaceList.push_back(ci);
          }
        }
      }
    }

    for (CallInst *ci : replaceList) {
      auto *op0 = cast<Value>(ci->getOperand(0));
      auto *op1 = cast<Value>(ci->getOperand(1));

      // replace call
      FunctionType *type = ci->getCalledFunction()->getFunctionType();
      FunctionCallee memCallee;
      const char* tapirTarget = std::getenv("NM_TAPIRTARGET");
      if (strcmp(tapirTarget, "opencilk") == 0) {
        memCallee = m->getOrInsertFunction("__kitcuda_mem_alloc_managed_numba_oc", type);
      } else {
        memCallee = m->getOrInsertFunction("__kitcuda_mem_alloc_managed_numba_cu", type);
      }
      CallInst *newCall = CallInst::Create(memCallee, {op0, op1});
      ReplaceInstWithInst(ci, newCall);
    }
    return PreservedAnalyses::none();
  }

  static bool isRequired() { return true; }
};
} // namespace llvm

static void runOptimizationPasses(llvm::Module &M, llvm::TargetMachine &TM) {
  llvm::CodeGenOptLevel optLevelVal = TM.getOptLevel();
  // this needs to be modified for other users/systems
  std::string bitcodeFile = "/vast/home/josephsarrao/kitinstall_t/lib/clang/18/lib/x86_64-unknown-linux-gnu/libopencilk-abi.bc";

  llvm::errs() << "TM CodeGenOptLevel: " << (int) TM.getOptLevel() << '\n';

  for (llvm::Function &func : M.functions()) {
    if (func.getName() == "nmrtCreateAllocToken") {
      func.addRetAttr(llvm::Attribute::AttrKind::NoAlias);
      for (llvm::Use &use : func.uses()) {
        if (auto *call = llvm::dyn_cast<llvm::CallInst>(use.getUser())) {
          if (call->getCalledFunction() == &func) {
            call->addRetAttr(llvm::Attribute::AttrKind::NoAlias);
          }
        }
      }
    }
  }

  // create first pass manager which will run O1, replaceNRTAllocPass, and
  // taprifyLoopPass
  llvm::PipelineTuningOptions PTO;
  PTO.LoopUnrolling = false;
  PTO.LoopVectorization = false;
  PTO.LoopStripmine = false;
  llvm::PassBuilder PB1(&TM, PTO);

  llvm::Triple ModuleTriple(M.getTargetTriple());
  llvm::TargetLibraryInfoImpl TLII(ModuleTriple);
  const char* tapirTarget = std::getenv("NM_TAPIRTARGET");
  if (strcmp(tapirTarget, "opencilk") == 0) {
    TLII.setTapirTarget(llvm::TapirTargetID::OpenCilk);
    TLII.setTapirTargetOptions(std::make_unique<llvm::OpenCilkABIOptions>(bitcodeFile));
  } else {
    TLII.setTapirTarget(llvm::TapirTargetID::Cuda);
  }
  TLII.addTapirTargetLibraryFunctions();

  llvm::LoopAnalysisManager LAM1;
  llvm::FunctionAnalysisManager FAM1;
  llvm::CGSCCAnalysisManager CGAM1;
  llvm::ModuleAnalysisManager MAM1;
  FAM1.registerPass([&] { return llvm::TargetLibraryAnalysis(TLII); });

  PB1.registerModuleAnalyses(MAM1);
  PB1.registerCGSCCAnalyses(CGAM1);
  PB1.registerFunctionAnalyses(FAM1);
  PB1.registerLoopAnalyses(LAM1);
  PB1.crossRegisterProxies(LAM1, FAM1, CGAM1, MAM1);


  llvm::ModulePassManager MPM1 = PB1.buildPerModuleDefaultPipeline(
      llvm::OptimizationLevel::O1, false, TLII.hasTapirTarget());
  MPM1.addPass(
      llvm::createModuleToFunctionPassAdaptor(llvm::replaceNRTAllocPass()));
  MPM1.addPass(
      llvm::createModuleToFunctionPassAdaptor(llvm::tapirifyLoopPass()));
  MPM1.run(M, MAM1);

  // create second pass manager which will run optimization pass for optLevelVal
  // (usually O2)
  llvm::PipelineTuningOptions PTO2 = getPipelineTuningOptions(optLevelVal);
  if (strcmp(tapirTarget, "opencilk") == 0) {
    PTO2.LoopUnrolling = true;
    PTO2.LoopVectorization = false;
    PTO2.LoopStripmine = true;
  } else {
    PTO2.LoopUnrolling = false;
    PTO2.LoopVectorization = false;
    PTO2.LoopStripmine = false;
  }
  llvm::PassBuilder PB2(&TM, PTO2);

  llvm::TargetLibraryInfoImpl TLII2(ModuleTriple);
  if (strcmp(tapirTarget, "opencilk") == 0) {
    TLII2.setTapirTarget(llvm::TapirTargetID::OpenCilk);
    TLII2.setTapirTargetOptions(std::make_unique<llvm::OpenCilkABIOptions>(bitcodeFile));
  } else {
    TLII2.setTapirTarget(llvm::TapirTargetID::Cuda);
  }
  TLII2.addTapirTargetLibraryFunctions();

  llvm::LoopAnalysisManager LAM2;
  llvm::FunctionAnalysisManager FAM2;
  llvm::CGSCCAnalysisManager CGAM2;
  llvm::ModuleAnalysisManager MAM2;
  FAM2.registerPass([&] { return llvm::TargetLibraryAnalysis(TLII2); });

  llvm::PassInstrumentationCallbacks PIC;
  llvm::PrintPassOptions PPO;
  PPO.Indent = false;
  PPO.SkipAnalyses = false;
  llvm::StandardInstrumentations SI(M.getContext(), /*debugLogging*/ false,
                                    /*verifyEach*/ true, PPO);
  SI.registerCallbacks(PIC, &MAM2);

  // Register all the basic analyses with the managers.
  PB2.registerModuleAnalyses(MAM2);
  PB2.registerCGSCCAnalyses(CGAM2);
  PB2.registerFunctionAnalyses(FAM2);
  PB2.registerLoopAnalyses(LAM2);
  PB2.crossRegisterProxies(LAM2, FAM2, CGAM2, MAM2);

  llvm::OptimizationLevel level = mapToLevel(optLevelVal);
  llvm::ModulePassManager MPM2;
  MPM2 =
      PB2.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O3, false, TLII2.hasTapirTarget());
  MPM2.run(M, MAM2);
}

/// A simple object cache following Lang's LLJITWithObjectCache example.
class numba::ExecutionEngine::SimpleObjectCache : public llvm::ObjectCache {
public:
  void notifyObjectCompiled(const llvm::Module *m,
                            llvm::MemoryBufferRef objBuffer) override {
    cachedObjects[m->getModuleIdentifier()] =
        llvm::MemoryBuffer::getMemBufferCopy(objBuffer.getBuffer(),
                                             objBuffer.getBufferIdentifier());
  }

  std::unique_ptr<llvm::MemoryBuffer>
  getObject(const llvm::Module *m) override {
    auto i = cachedObjects.find(m->getModuleIdentifier());
    if (i == cachedObjects.end()) {
      LLVM_DEBUG(llvm::dbgs() << "No object for " << m->getModuleIdentifier()
                              << " in cache. Compiling.\n");
      return nullptr;
    }
    LLVM_DEBUG(llvm::dbgs() << "Object for " << m->getModuleIdentifier()
                            << " loaded from cache.\n");
    return llvm::MemoryBuffer::getMemBuffer(i->second->getMemBufferRef());
  }

  /// Dump cached object to output file `filename`.
  void dumpToObjectFile(llvm::StringRef outputFilename) {
    // Set up the output file.
    std::string errorMessage;
    auto file = mlir::openOutputFile(outputFilename, &errorMessage);
    if (!file) {
      llvm::errs() << errorMessage << "\n";
      return;
    }

    // Dump the object generated for a single module to the output file.
    assert(cachedObjects.size() == 1 && "Expected only one object entry.");
    auto &cachedObject = cachedObjects.begin()->second;
    file->os() << cachedObject->getBuffer();
    file->keep();
  }

private:
  llvm::StringMap<std::unique_ptr<llvm::MemoryBuffer>> cachedObjects;
};

/// Wrap a string into an llvm::StringError.
static llvm::Error makeStringError(const llvm::Twine &message) {
  return llvm::make_error<llvm::StringError>(message.str(),
                                             llvm::inconvertibleErrorCode());
}

// Setup LLVM target triple from the current machine.
static void setupModule(llvm::Module &M, llvm::TargetMachine &TM) {
  M.setDataLayout(TM.createDataLayout());
  M.setTargetTriple(TM.getTargetTriple().normalize());
  for (auto &&func : M.functions()) {
    if (!func.hasFnAttribute("target-cpu"))
      func.addFnAttr("target-cpu", TM.getTargetCPU());

    if (!func.hasFnAttribute("target-features")) {
      auto featStr = TM.getTargetFeatureString();
      if (!featStr.empty())
        func.addFnAttr("target-features", featStr);
    }
  }
}

namespace {
class CustomCompiler : public llvm::orc::SimpleCompiler {
public:
  using Transformer = std::function<llvm::Error(llvm::Module &)>;
  using AsmPrinter = std::function<void(llvm::StringRef)>;

  CustomCompiler(Transformer t, AsmPrinter a,
                 std::unique_ptr<llvm::TargetMachine> TM,
                 llvm::ObjectCache *ObjCache = nullptr)
      : SimpleCompiler(*TM, ObjCache), TM(std::move(TM)),
        transformer(std::move(t)), printer(std::move(a)) {}

  llvm::Expected<CompileResult> operator()(llvm::Module &M) override {
    if (transformer) {
      auto err = transformer(M);
      if (err)
        return err;
    }

    setupModule(M, *TM);
    runOptimizationPasses(M, *TM);

    if (printer) {
      llvm::SmallVector<char, 0> buffer;
      llvm::raw_svector_ostream os(buffer);

      llvm::legacy::PassManager PM;
      if (TM->addPassesToEmitFile(PM, os, nullptr,
                                  llvm::CodeGenFileType::AssemblyFile))
        return makeStringError("Target does not support Asm emission");

      PM.run(M);
      printer(llvm::StringRef(buffer.data(), buffer.size()));
    }

    return llvm::orc::SimpleCompiler::operator()(M);
  }

private:
  std::shared_ptr<llvm::TargetMachine> TM;
  Transformer transformer;
  AsmPrinter printer;
};
} // namespace

numba::ExecutionEngine::ExecutionEngine(ExecutionEngineOptions options)
    : cache(options.enableObjectCache ? new SimpleObjectCache() : nullptr),
      gdbListener(options.enableGDBNotificationListener
                      ? llvm::JITEventListener::createGDBRegistrationListener()
                      : nullptr),
      perfListener(nullptr) {
  if (options.enablePerfNotificationListener) {
    if (auto *listener = llvm::JITEventListener::createPerfJITEventListener())
      perfListener = listener;
    else if (auto *listener =
                 llvm::JITEventListener::createIntelJITEventListener())
      perfListener = listener;
  }

  // Callback to create the object layer with symbol resolution to current
  // process and dynamically linked libraries.
  auto objectLinkingLayerCreator = [this](llvm::orc::ExecutionSession &session,
                                          const llvm::Triple &targetTriple) {
    auto objectLayer =
        std::make_unique<llvm::orc::RTDyldObjectLinkingLayer>(session, []() {
          return std::make_unique<llvm::SectionMemoryManager>();
        });

    // Register JIT event listeners if they are enabled.
    if (gdbListener)
      objectLayer->registerJITEventListener(*gdbListener);
    if (perfListener)
      objectLayer->registerJITEventListener(*perfListener);

    // COFF format binaries (Windows) need special handling to deal with
    // exported symbol visibility.
    // cf llvm/lib/ExecutionEngine/Orc/LLJIT.cpp LLJIT::createObjectLinkingLayer
    if (targetTriple.isOSBinFormatCOFF()) {
      objectLayer->setOverrideObjectFlagsWithResponsibilityFlags(true);
      objectLayer->setAutoClaimResponsibilityForObjectSymbols(true);
    }

    return objectLayer;
  };

  // Callback to inspect the cache and recompile on demand. This follows Lang's
  // LLJITWithObjectCache example.
  auto compileFunctionCreator =
      [this, jitCodeGenOptLevel = options.jitCodeGenOptLevel,
       transformer = options.lateTransformer,
       asmPrinter = options.asmPrinter](llvm::orc::JITTargetMachineBuilder jtmb)
      -> llvm::Expected<
          std::unique_ptr<llvm::orc::IRCompileLayer::IRCompiler>> {
    if (jitCodeGenOptLevel)
      jtmb.setCodeGenOptLevel(*jitCodeGenOptLevel);
    auto tm = jtmb.createTargetMachine();
    if (!tm)
      return tm.takeError();
    return std::make_unique<CustomCompiler>(transformer, asmPrinter,
                                            std::move(*tm), cache.get());
  };

  auto tmBuilder =
      llvm::cantFail(llvm::orc::JITTargetMachineBuilder::detectHost());

  // Create the LLJIT by calling the LLJITBuilder with 2 callbacks.
  jit = cantFail(llvm::orc::LLJITBuilder()
                     .setCompileFunctionCreator(compileFunctionCreator)
                     .setObjectLinkingLayerCreator(objectLinkingLayerCreator)
                     .setJITTargetMachineBuilder(tmBuilder)
                     .create());

  symbolMap = std::move(options.symbolMap);
  transformer = std::move(options.transformer);
}

numba::ExecutionEngine::~ExecutionEngine() {}

llvm::Expected<numba::ExecutionEngine::ModuleHandle>
numba::ExecutionEngine::loadModule(mlir::ModuleOp m) {
  assert(m);

  std::unique_ptr<llvm::LLVMContext> ctx(new llvm::LLVMContext);
  auto llvmModule = mlir::translateModuleToLLVMIR(m, *ctx);

  // options that kitsune likes, useful to mess around with these in the
  // event of strange behavior
  llvmModule->setCodeModel(llvm::CodeModel::Large);
  llvmModule->setPICLevel(llvm::PICLevel::BigPIC);
  llvmModule->setPIELevel(llvm::PIELevel::Large);
  llvmModule->setDirectAccessExternalData(true);

  if (!llvmModule)
    return makeStringError("could not convert to LLVM IR");

  // Add a ThreadSafemodule to the engine and return.
  llvm::orc::ThreadSafeModule tsm(std::move(llvmModule), std::move(ctx));
  if (transformer)
    cantFail(tsm.withModuleDo(
        [this](llvm::Module &module) { return transformer(module); }));

  llvm::orc::JITDylib *dylib;
  while (true) {
    auto uniqueName =
        (llvm::Twine("module") + llvm::Twine(uniqueNameCounter++)).str();
    if (jit->getJITDylibByName(uniqueName))
      continue;

    auto res = jit->createJITDylib(std::move(uniqueName));
    if (!res)
      return res.takeError();

    dylib = &res.get();
    break;
  }
  assert(dylib);

  auto dataLayout = jit->getDataLayout();
  dylib->addGenerator(
      cantFail(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
          dataLayout.getGlobalPrefix())));

  if (symbolMap)
    cantFail(
        dylib->define(absoluteSymbols(symbolMap(llvm::orc::MangleAndInterner(
            dylib->getExecutionSession(), jit->getDataLayout())))));

  llvm::cantFail(jit->addIRModule(*dylib, std::move(tsm)));

  // add kitsune cuda functions to symbol map
  llvm::SmallVector<std::string> kitcudaFns{
      "__cudaRegisterFatBinary",
      "__cudaRegisterFatBinaryEnd",
      "__cudaUnregisterFatBinary",
      "__kitcuda_use_occupancy_launch",
      "__kitcuda_initialize",
      "__kitcuda_destroy",
      "__kitcuda_launch_kernel",
      "__kitcuda_mem_gpu_prefetch",
      "__kitcuda_set_default_threads_per_blk",
      "__kitcuda_sync_thread_stream",
      "__kitcuda_mem_alloc_managed_numba_cu",
      "__kitcuda_mem_alloc_managed_numba_oc"};
  llvm::orc::MangleAndInterner Mangle(dylib->getExecutionSession(),
                                      jit->getDataLayout());
  static void *dlHandle = nullptr;
  llvm::DenseMap<llvm::orc::SymbolStringPtr, llvm::orc::ExecutorSymbolDef>
      symMap;
  // this needs to be modified for other users/systems
  if ((dlHandle = dlopen(
           "/vast/home/josephsarrao/kitinstall_t/lib/clang/18/lib/libkitrt.so",
           RTLD_LAZY))) {
    for (auto fn : kitcudaFns) {
      if (void *funcAddr = dlsym(dlHandle, fn.c_str())) {
        llvm::JITSymbolFlags flags;
        llvm::orc::ExecutorSymbolDef symDef(
            llvm::orc::ExecutorAddr::fromPtr(funcAddr), flags);
        symMap.insert({Mangle(fn.c_str()), std::move(symDef)});
      } else {
        llvm::report_fatal_error(
            "error finding kitcuda function in libkitrt.so\n");
      }
    }
  } else {
    llvm::errs() << "Could not find dlHandle for libkitrt.so\n";
  }

  // add kitsune opencilk functions to symbol map
  llvm::SmallVector<std::string> openCilkFns{
      "Cilk_exception_handler",
      "Cilk_set_return",
      "cilkg_nproc",
      "__pedigree_dprng_m_array",
      "__cilkrts_check_exception_raise",
      "__cilkrts_cleanup_fiber",
      "__cilkrts_internal_exit_cilkified_root",
      "__cilkrts_internal_invoke_cilkified_root",
      "__cilkrts_need_to_cilkify",
      "__cilkrts_sync",
      "__cilkrts_use_extension",
      "__emutls_v.__cilkrts_current_fh"};
  dlHandle = nullptr;
  // this needs to be modified for other users/systems
  if ((dlHandle = dlopen(
           "/vast/home/josephsarrao/kitinstall_t/lib/clang/18/lib/x86_64-unknown-linux-gnu/libopencilk.so",
           RTLD_LAZY))) {
    for (auto fn : openCilkFns) {
      if (void *funcAddr = dlsym(dlHandle, fn.c_str())) {
        llvm::JITSymbolFlags flags;
        llvm::orc::ExecutorSymbolDef symDef(
            llvm::orc::ExecutorAddr::fromPtr(funcAddr), flags);
        symMap.insert({Mangle(fn.c_str()), std::move(symDef)});
      } else {
        llvm::report_fatal_error(
            "error finding opencilk function in libopencilk.so\n");
      }
    }
  } else {
    llvm::errs() << "Could not find dlHandle for libopencilk.so\n";
  }

  // add opencilk personality functions to symbol map
  llvm::SmallVector<std::string> openCilkPersFns{
      "__cilk_personality_v0"};
  dlHandle = nullptr;
  // this needs to be modified for other users/systems
  if ((dlHandle = dlopen(
           "/vast/home/josephsarrao/kitinstall_t/lib/clang/18/lib/x86_64-unknown-linux-gnu/libopencilk-personality-c.so",
           RTLD_LAZY))) {
    for (auto fn : openCilkPersFns) {
      if (void *funcAddr = dlsym(dlHandle, fn.c_str())) {
        llvm::JITSymbolFlags flags;
        llvm::orc::ExecutorSymbolDef symDef(
            llvm::orc::ExecutorAddr::fromPtr(funcAddr), flags);
        symMap.insert({Mangle(fn.c_str()), std::move(symDef)});
      } else {
        llvm::report_fatal_error(
            "error finding opencilk function in libopencilk-personality-c.so\n");
      }
    }
  } else {
    llvm::errs() << "Could not find dlHandle for libopencilk-personality-cpp.so\n";
  }

  // add kitsune timer functions to symbol map
  llvm::SmallVector<std::string> timerFns{
      "startKitTimer",
      "endKitTimer"};
  dlHandle = nullptr;
  // this needs to be modified for other users/systems
  if ((dlHandle = dlopen(
           "/vast/home/josephsarrao/python_experiments/numba-mlir/yw_therm/timerFuncs.so",
           RTLD_LAZY))) {
    for (auto fn : timerFns) {
      if (void *funcAddr = dlsym(dlHandle, fn.c_str())) {
        llvm::JITSymbolFlags flags;
        llvm::orc::ExecutorSymbolDef symDef(
            llvm::orc::ExecutorAddr::fromPtr(funcAddr), flags);
        symMap.insert({Mangle(fn.c_str()), std::move(symDef)});
      } else {
        // llvm::errs() << fn << '\n';
        llvm::report_fatal_error(
            "error finding timer func\n");
      }
    }
  } else {
    llvm::errs() << "Could not find dlHandle for timerFuncs.so\n";
  }

  llvm::cantFail(dylib->define(llvm::orc::absoluteSymbols(symMap)));
  llvm::cantFail(jit->initialize(*dylib));
  return static_cast<ModuleHandle>(dylib);
}

void numba::ExecutionEngine::releaseModule(ModuleHandle handle) {
  assert(handle);
  auto dylib = static_cast<llvm::orc::JITDylib *>(handle);
  llvm::cantFail(jit->deinitialize(*dylib));
  llvm::cantFail(jit->getExecutionSession().removeJITDylib(*dylib));
}

llvm::Expected<void *>
numba::ExecutionEngine::lookup(numba::ExecutionEngine::ModuleHandle handle,
                               llvm::StringRef name) const {
  assert(handle);
  auto dylib = static_cast<llvm::orc::JITDylib *>(handle);
  auto expectedSymbol = jit->lookup(*dylib, name);

  // JIT lookup may return an Error referring to strings stored internally by
  // the JIT. If the Error outlives the ExecutionEngine, it would want have a
  // dangling reference, which is currently caught by an assertion inside JIT
  // thanks to hand-rolled reference counting. Rewrap the error message into a
  // string before returning. Alternatively, ORC JIT should consider copying
  // the string into the error message.
  if (!expectedSymbol) {
    std::string errorMessage;
    llvm::raw_string_ostream os(errorMessage);
    llvm::handleAllErrors(expectedSymbol.takeError(),
                          [&os](llvm::ErrorInfoBase &ei) { ei.log(os); });
    return makeStringError(os.str());
  }

  if (void *fptr = expectedSymbol->toPtr<void *>())
    return fptr;

  return makeStringError("looked up function is null");
}

void numba::ExecutionEngine::dumpToObjectFile(llvm::StringRef filename) {
  if (cache == nullptr) {
    llvm::errs() << "cannot dump ExecutionEngine object code to file: "
                    "object cache is disabled\n";
    return;
  }
  cache->dumpToObjectFile(filename);
}
