// SPDX-FileCopyrightText: 2022 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "numba/ExecutionEngine/ExecutionEngine.hpp"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/GraphTraits.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/iterator.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/LoopNestAnalysis.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/MemorySSAUpdater.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include <llvm/ExecutionEngine/JITEventListener.h>
#include "llvm/ExecutionEngine/JITSymbol.h"
#include <llvm/ExecutionEngine/ObjectCache.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>
#include <llvm/ExecutionEngine/Orc/IRTransformLayer.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include "llvm/ExecutionEngine/Orc/Shared/ExecutorAddress.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/TapirUtils.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Value.h"
#include <llvm/MC/TargetRegistry.h>
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include <llvm/Support/MemoryBuffer.h>
#include "llvm/Support/raw_ostream.h"
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Target/TargetMachine.h>
#include "llvm/Transforms/Utils/ValueMapper.h"

#include <mlir/IR/BuiltinOps.h>
#include <mlir/Support/FileUtilities.h>
#include <mlir/Target/LLVMIR/Export.h>

#include <llvm/IR/PassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/PassBuilder.h>
#include "llvm/Passes/PassPlugin.h"
#include <llvm/Passes/StandardInstrumentations.h>

#include <cassert>
#include <cstddef>
#include <iterator>
#include <dlfcn.h>


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
  void splitLoop(Loop *l, Function &f, ScalarEvolution &se) {
    // create list of all phi nodes in the loops header block. one of these
    // should be the loop induction variable
    BasicBlock *header = l->getHeader();
    Module *m = f.getParent();
    SmallVector<PHINode *, 0> PHICandidates;
    for (PHINode &pn : header->phis()) {
      if (pn.getNumIncomingValues() == 2) {
        for (uint i = 0; i < 2; i++) {
          if (ConstantInt *CI = dyn_cast<ConstantInt>(pn.getIncomingValue(i))) {
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
      return;
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
        auto *operand0 = userI->getOperand(0);
        auto *operand1 = userI->getOperand(1);
        if (isa<ConstantInt>(operand0)) {
          // verify that the constantint operand has a value of 1
          auto *o0Val = dyn_cast<ConstantInt>(operand0);
          if (o0Val->getSExtValue() == 1) {
            increment = userI;
          }
        }
        if (isa<ConstantInt>(operand1)) {
          // verify that the constantint operand has a value of 1
          auto *o1Val = dyn_cast<ConstantInt>(operand1);
          if (o1Val->getSExtValue() == 1) {
            increment = userI;
          }
        }
      }
    }
    if (!increment) {
      return;
    }

    // find compare instruction that uses the increment instruction

    Instruction *icmp;
    for (Use &use : increment->uses()) {
      auto *user = use.getUser();
      Instruction *userI = dyn_cast<Instruction>(user);
      if (isa<ICmpInst>(userI)) {
        icmp = userI;
      }
    }
    if (!icmp) {
      return;
    }

    // find branch instruction that uses compare instruction
    Instruction *branch;
    for (Use &use : icmp->uses()) {
      auto *user = use.getUser();
      Instruction *userI = dyn_cast<Instruction>(user);
      branch = userI;
    }
    if (!branch) {
      return;
    }

    // now we need to split the blocks so that we can add in the tapir
    // instructions
    Instruction *firstSplitPoint;
    Instruction *secondSplitPoint;

    // while the compare always ends up after the loop body, sometimes the
    // increment can come before the loop body
    if (increment->getNextNonDebugInstruction() == icmp) {
      firstSplitPoint = header->getFirstNonPHI();
      secondSplitPoint = increment;
    } else {
      firstSplitPoint = increment->getNextNonDebugInstruction();
      secondSplitPoint = icmp;
    }
    if (!firstSplitPoint || !secondSplitPoint) {
      return;
    }

    // splitting at the first split point
    BasicBlock *parent1 = firstSplitPoint->getParent();
    BasicBlock *body = parent1->splitBasicBlock(firstSplitPoint, "body", false);
    if (body == nullptr) {
      return;
    }

    auto *parent2 = secondSplitPoint->getParent();
    BasicBlock *latch =
        parent2->splitBasicBlock(secondSplitPoint, "latch", false);
    if (latch == nullptr) {
      return;
    }

    // adding in tapir instructions
    // creating sync region
    FunctionType *type =
        FunctionType::get(Type::getTokenTy(m->getContext()), {}, false);
    FunctionCallee syncStart =
        m->getOrInsertFunction("llvm.syncregion.start", type);
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
    Instruction *exitFirstInst = exit->getFirstNonPHI();
    BasicBlock *newExitBlock =
        exit->splitBasicBlock(exitFirstInst, "exit", false);
    Instruction *syncTerm = exit->getTerminator();
    Instruction *syncInst =
        SyncInst::Create(newExitBlock, syncRegInst, syncTerm);
    syncTerm->eraseFromParent();

    // add neccessary tapir metadata to the loop
    auto *Int32Ty = Type::getInt32Ty(branch->getContext());
    SmallVector<Metadata *, 2> Ops;
    Ops.push_back(
        MDString::get(branch->getContext(), "tapir.loop.spawn.strategy"));
    Ops.push_back(ConstantAsMetadata::get(ConstantInt::get(Int32Ty, TapirLoopHints::SpawningStrategy::ST_DAC)));
    SmallVector<Metadata *, 2> targetMD;
    targetMD.push_back(MDString::get(branch->getContext(), "tapir.loop.target"));
    targetMD.push_back(ConstantAsMetadata::get(ConstantInt::get(Int32Ty, (uint64_t) TapirTargetID::Cuda)));
    auto *node = MDTuple::get(branch->getContext(), Ops);
    SmallVector<Metadata *, 2> nullMD;
    auto *branchMD = MDNode::getDistinct(branch->getContext(), nullMD);
    auto *tapirNode = MDNode::get(branch->getContext(), Ops);
    auto *targetNode = MDNode::get(branch->getContext(), targetMD);
    branchMD->push_back(branchMD);
    branchMD->push_back(tapirNode);
    branchMD->push_back(targetNode);
    
    branch->setMetadata("llvm.loop", branchMD);
  }

  PreservedAnalyses run(Function &f, FunctionAnalysisManager &am) {
    auto &li = am.getResult<LoopAnalysis>(f);
    auto &se = am.getResult<ScalarEvolutionAnalysis>(f);
    for (auto l : li) {
      LoopNest ln(*l, se);
      int numNested = (int)ln.getNumLoops();
      for (int i = numNested - 1; i >= 0; i--) {
        auto *nestedLoop = ln.getLoop(i);
        splitLoop(nestedLoop, f, se);
      }
    }
    return PreservedAnalyses::none();
  }

  static bool isRequired() { return true; }
};

struct replaceNRTAllocPass : PassInfoMixin<replaceNRTAllocPass> {
  PreservedAnalyses run(Function &f, FunctionAnalysisManager &am) {
    Module *m = f.getParent();

    FunctionType *printType = FunctionType::get(Type::getVoidTy(m->getContext()), {Type::getInt64Ty(m->getContext())}, false);
    FunctionCallee printFuncCallee = m->getOrInsertFunction("llvmPrintI64", printType);

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
      FunctionCallee memCallee = m->getOrInsertFunction("__kitcuda_mem_alloc_managed_numba", type);
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

  llvm::PipelineTuningOptions PTO;
  PTO.LoopUnrolling = false;
  PTO.LoopVectorization = false;
  PTO.LoopStripmine = false;

  llvm::PassBuilder pb1(&TM, PTO);

  llvm::Triple ModuleTriple(M.getTargetTriple());
  llvm::TargetLibraryInfoImpl TLII(ModuleTriple);
  TLII.setTapirTarget(llvm::TapirTargetID::Cuda);
  TLII.addTapirTargetLibraryFunctions();

  llvm::LoopAnalysisManager lam1;
  llvm::FunctionAnalysisManager fam1;
  llvm::CGSCCAnalysisManager cgam1;
  llvm::ModuleAnalysisManager mam1;
  fam1.registerPass([&] { return llvm::TargetLibraryAnalysis(TLII); });

  pb1.registerModuleAnalyses(mam1);
  pb1.registerCGSCCAnalyses(cgam1);
  pb1.registerFunctionAnalyses(fam1);
  pb1.registerLoopAnalyses(lam1);
  pb1.crossRegisterProxies(lam1, fam1, cgam1, mam1);

  llvm::ModulePassManager mpm1 = pb1.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O1, false, TLII.hasTapirTarget());
  mpm1.addPass(llvm::createModuleToFunctionPassAdaptor(llvm::replaceNRTAllocPass()));
  mpm1.addPass(llvm::createModuleToFunctionPassAdaptor(llvm::tapirifyLoopPass()));

  mpm1.run(M, mam1);


  llvm::LoopAnalysisManager lam;
  llvm::FunctionAnalysisManager fam;
  llvm::CGSCCAnalysisManager cgam;
  llvm::ModuleAnalysisManager mam;

  llvm::TargetLibraryInfoImpl TLII2(ModuleTriple);
  TLII2.setTapirTarget(llvm::TapirTargetID::Cuda);
  TLII2.addTapirTargetLibraryFunctions();
  fam.registerPass([&] { return llvm::TargetLibraryAnalysis(TLII2); });

  llvm::PassInstrumentationCallbacks pic;
  llvm::PrintPassOptions ppo;
  ppo.Indent = false;
  ppo.SkipAnalyses = false;
  llvm::StandardInstrumentations si(M.getContext(), /*debugLogging*/ false,
                                    /*verifyEach*/ true, ppo);

  si.registerCallbacks(pic, &mam);

  llvm::PipelineTuningOptions PTO2 = getPipelineTuningOptions(optLevelVal);
  PTO2.LoopUnrolling = false;
  PTO2.LoopVectorization = false;
  PTO2.LoopStripmine = false;
  llvm::PassBuilder pb(&TM, PTO2);


  llvm::ModulePassManager mpm;

  if (/*verify*/ true) {
    pb.registerPipelineStartEPCallback(
        [&](llvm::ModulePassManager &mpm, llvm::OptimizationLevel level) {
          mpm.addPass(createModuleToFunctionPassAdaptor(llvm::VerifierPass()));
        });
  }



  // Register all the basic analyses with the managers.
  pb.registerModuleAnalyses(mam);
  pb.registerCGSCCAnalyses(cgam);
  pb.registerFunctionAnalyses(fam);
  pb.registerLoopAnalyses(lam);
  pb.crossRegisterProxies(lam, fam, cgam, mam);

  llvm::OptimizationLevel level = mapToLevel(optLevelVal);

  if (optLevelVal == llvm::CodeGenOptLevel::None) {
    mpm = pb.buildO0DefaultPipeline(level);
  } else {
    if (TLII.hasTapirTarget()) {
      llvm::errs() << "tlii hastapirtarget = true\n";
    }
    mpm = pb.buildPerModuleDefaultPipeline(level, false, TLII.hasTapirTarget());
  }

  mpm.run(M, mam);
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

  llvmModule->setCodeModel(llvm::CodeModel::Large);
  llvmModule->setPICLevel(llvm::PICLevel::BigPIC);
  llvmModule->setPIELevel(llvm::PIELevel::Large);
  llvmModule->setDirectAccessExternalData(true);

  llvm::DataLayout llvmDL = llvmModule->getDataLayout();
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

  llvm::SmallVector<std::string> kitcudaFns {"__cudaRegisterFatBinary", "__cudaRegisterFatBinaryEnd", "__cudaUnregisterFatBinary", "__kitcuda_use_occupancy_launch", "__kitcuda_initialize", "__kitcuda_destroy", "__kitcuda_launch_kernel", "__kitcuda_mem_gpu_prefetch", "__kitcuda_set_default_threads_per_blk", "__kitcuda_sync_thread_stream", "__kitcuda_mem_alloc_managed_numba"};
  llvm::orc::MangleAndInterner Mangle(dylib->getExecutionSession(), jit->getDataLayout());

  static void *dlHandle = nullptr;
  llvm::DenseMap<llvm::orc::SymbolStringPtr, llvm::orc::ExecutorSymbolDef> symMap;
  if ((dlHandle = dlopen("/vast/home/josephsarrao/kitinstall_t/lib/clang/18/lib/libkitrt.so", RTLD_LAZY))) {
    for (auto fn : kitcudaFns) {
      if (void *funcAddr = dlsym(dlHandle, fn.c_str())) {
        
        llvm::JITSymbolFlags flags;
        llvm::orc::ExecutorSymbolDef symDef(llvm::orc::ExecutorAddr::fromPtr(funcAddr), flags);
        
        symMap.insert({Mangle(fn.c_str()), std::move(symDef)});
        
      } else {
        llvm::report_fatal_error("error finding kitcuda function in libkitrt.so\n");
      }
    }
  } else {
      llvm::errs() << "Could not find dlHandle for libkitrt.so\n";
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
