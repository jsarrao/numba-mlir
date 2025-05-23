// SPDX-FileCopyrightText: 2021 - 2022 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "pipelines/ParallelToTbb.hpp"

#include <mlir/Dialect/Arith/IR/Arith.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/Dialect/MemRef/IR/MemRef.h>
#include <mlir/Dialect/SCF/IR/SCF.h>
#include <mlir/IR/IRMapping.h>
#include <mlir/Pass/Pass.h>
#include <mlir/Pass/PassManager.h>
#include <mlir/Transforms/GreedyPatternRewriteDriver.h>
#include <mlir/Transforms/Passes.h>

#include "pipelines/BasePipeline.hpp"
#include "pipelines/LowerToLlvm.hpp"

#include "numba/Compiler/PipelineRegistry.hpp"
#include "numba/Dialect/numba_util/Dialect.hpp"
#include "numba/Transforms/FuncUtils.hpp"
#include "numba/Transforms/RewriteWrapper.hpp"
#include "numba/Transforms/SCFVectorize.hpp"

namespace {
static mlir::MemRefType getReduceType(mlir::Type type, int64_t count) {
  if (type.isIntOrFloat())
    return mlir::MemRefType::get(count, type);

  return {};
}

static std::optional<mlir::TypedAttr>
getReduceInitVal(mlir::Type type, mlir::Block &reduceBlock) {
  if (!llvm::hasSingleElement(reduceBlock.without_terminator()))
    return std::nullopt;

  return mlir::arith::getNeutralElement(&(*reduceBlock.begin()));
}

static bool isInsideParalleRegion(mlir::Operation *op) {
  // Must be direct parent
  auto region =
      mlir::dyn_cast<numba::util::EnvironmentRegionOp>(op->getParentOp());
  return region &&
         mlir::isa<numba::util::ParallelAttr>(region.getEnvironment());
}

struct ParallelToTbb : public mlir::OpRewritePattern<mlir::scf::ParallelOp> {
  using OpRewritePattern::OpRewritePattern;

  mlir::LogicalResult
  matchAndRewrite(mlir::scf::ParallelOp op,
                  mlir::PatternRewriter &rewriter) const override {
    if (mlir::isa<numba::util::ParallelOp>(op->getParentOp()))
      return mlir::failure();

    bool needParallel = isInsideParalleRegion(op) ||
                        !op->getParentOfType<mlir::scf::ParallelOp>();
    if (!needParallel)
      return mlir::failure();

    int64_t maxConcurrency = 0;
    auto func = op->getParentOfType<mlir::func::FuncOp>();
    if (!func)
      return mlir::failure();
    if (auto mc = func->getAttrOfType<mlir::IntegerAttr>(
            numba::util::attributes::getMaxConcurrencyName()))
      maxConcurrency = mc.getInt();

    if (maxConcurrency <= 1)
      return mlir::failure();

    for (auto type : op.getResultTypes())
      if (!getReduceType(type, maxConcurrency))
        return mlir::failure();

    llvm::SmallVector<mlir::TypedAttr> initVals;
    initVals.reserve(op.getNumResults());
    auto reduceOp =
        mlir::cast<mlir::scf::ReduceOp>(op.getBody()->getTerminator());
    for (mlir::Region &region : reduceOp.getReductions()) {
      if (!llvm::hasSingleElement(region))
        return mlir::failure();

      auto ind = static_cast<unsigned>(initVals.size());
      auto reduceInitVal =
          getReduceInitVal(op.getResult(ind).getType(), region.front());
      if (!reduceInitVal)
        return mlir::failure();

      initVals.emplace_back(*reduceInitVal);
    }
    assert(initVals.size() == op.getNumResults());

    numba::AllocaInsertionPoint allocaIP(op);

    auto loc = op.getLoc();
    mlir::IRMapping mapping;
    llvm::SmallVector<mlir::Value> reduceVars(op.getNumResults());
    for (auto &&[i, type] : llvm::enumerate(op.getResultTypes())) {
      auto reduceType = getReduceType(type, maxConcurrency);
      assert(reduceType);
      auto reduce = allocaIP.insert(rewriter, [&]() {
        return rewriter.create<mlir::memref::AllocaOp>(loc, reduceType);
      });
      reduceVars[i] = reduce;
    }

    auto reduceInitBodyBuilder = [&](mlir::OpBuilder &builder,
                                     mlir::Location loc, mlir::Value index,
                                     mlir::ValueRange args) {
      assert(args.empty());
      (void)args;
      for (auto &&[i, reduce] : llvm::enumerate(reduceVars)) {
        auto initVal = initVals[i];
        auto init = builder.create<mlir::arith::ConstantOp>(loc, initVal);
        builder.create<mlir::memref::StoreOp>(loc, init, reduce, index);
      }
      builder.create<mlir::scf::YieldOp>(loc);
    };

    auto reduceLowerBound =
        rewriter.create<mlir::arith::ConstantIndexOp>(loc, 0);
    auto reduceUpperBound =
        rewriter.create<mlir::arith::ConstantIndexOp>(loc, maxConcurrency);
    auto reduceStep = rewriter.create<mlir::arith::ConstantIndexOp>(loc, 1);
    rewriter.create<mlir::scf::ForOp>(loc, reduceLowerBound, reduceUpperBound,
                                      reduceStep, std::nullopt,
                                      reduceInitBodyBuilder);

    auto origLowerBound = op.getLowerBound();
    auto origUpperBound = op.getUpperBound();
    auto origStep = op.getStep();
    auto bodyBuilder = [&](mlir::OpBuilder &builder, ::mlir::Location loc,
                           mlir::ValueRange lowerBound,
                           mlir::ValueRange upperBound,
                           mlir::Value threadIndex) {
      llvm::SmallVector<mlir::Value> initVals(op.getInitVals().size());
      for (auto &&[i, reduceVar] : llvm::enumerate(reduceVars)) {
        auto val =
            builder.create<mlir::memref::LoadOp>(loc, reduceVar, threadIndex);
        initVals[i] = val;
      }
      auto newOp =
          mlir::cast<mlir::scf::ParallelOp>(builder.clone(*op, mapping));
      assert(newOp->getNumResults() == reduceVars.size());
      newOp.getLowerBoundMutable().assign(lowerBound);
      newOp.getUpperBoundMutable().assign(upperBound);
      newOp.getInitValsMutable().assign(initVals);
      for (auto &&[i, val] : llvm::enumerate(newOp->getResults())) {
        auto reduceVar = reduceVars[i];
        builder.create<mlir::memref::StoreOp>(loc, val, reduceVar, threadIndex);
      }
    };

    rewriter.create<numba::util::ParallelOp>(
        loc, origLowerBound, origUpperBound, origStep, bodyBuilder);

    auto reduceBodyBuilder = [&](mlir::OpBuilder &builder, mlir::Location loc,
                                 mlir::Value index, mlir::ValueRange args) {
      assert(args.size() == reduceVars.size());
      mapping.clear();

      llvm::SmallVector<mlir::Value> yieldArgs;
      yieldArgs.reserve(args.size());
      for (auto &&[i, reduceOpRegion] :
           llvm::enumerate(reduceOp.getReductions())) {
        auto &reduceVar = reduceVars[i];
        auto arg = args[static_cast<unsigned>(i)];
        auto &reduceOpBody = reduceOpRegion.front();
        assert(reduceOpBody.getNumArguments() == 2);
        auto prevVal =
            builder.create<mlir::memref::LoadOp>(loc, reduceVar, index);
        mapping.map(reduceOpBody.getArgument(0), arg);
        mapping.map(reduceOpBody.getArgument(1), prevVal);
        for (auto &oldReduceOp : reduceOpBody.without_terminator())
          builder.clone(oldReduceOp, mapping);

        auto result =
            mlir::cast<mlir::scf::ReduceReturnOp>(reduceOpBody.getTerminator())
                .getResult();
        result = mapping.lookupOrNull(result);
        assert(result);
        yieldArgs.emplace_back(result);
      }
      builder.create<mlir::scf::YieldOp>(loc, yieldArgs);
    };

    auto reduceLoop = rewriter.create<mlir::scf::ForOp>(
        loc, reduceLowerBound, reduceUpperBound, reduceStep, op.getInitVals(),
        reduceBodyBuilder);
    rewriter.replaceOp(op, reduceLoop.getResults());

    return mlir::success();
  }
};

static bool
isAnyArgDefinedInsideRegions(llvm::MutableArrayRef<mlir::Region> regs,
                             mlir::Operation *op) {
  assert(op);
  for (auto arg : op->getOperands())
    for (auto &reg : regs)
      if (reg.isAncestor(arg.getParentRegion()))
        return true;

  return false;
}

struct LoopInfo {
  mlir::Operation *outermostLoop = nullptr;
  numba::util::ParallelOp innermostParallel;
};

static std::optional<LoopInfo> getLoopInfo(mlir::Operation *op) {
  assert(op);

  LoopInfo ret;
  auto parent = op->getParentOp();
  while (parent) {
    if (isAnyArgDefinedInsideRegions(parent->getRegions(), op))
      break;

    if (mlir::isa<mlir::scf::WhileOp, mlir::scf::ForOp, mlir::scf::ParallelOp,
                  numba::util::ParallelOp>(parent))
      ret.outermostLoop = parent;

    if (!ret.innermostParallel && mlir::isa<numba::util::ParallelOp>(parent))
      ret.innermostParallel = mlir::cast<numba::util::ParallelOp>(parent);

    parent = parent->getParentOp();
  }

  if (!ret.outermostLoop)
    return std::nullopt;

  return ret;
}

static bool canResultEscape(mlir::Operation *op, bool original = true) {
  for (auto user : op->getUsers()) {
    if (mlir::isa<mlir::memref::LoadOp, mlir::memref::StoreOp>(user))
      continue;

    if (original && mlir::isa<mlir::memref::DeallocOp>(user))
      continue;

    if (auto view = mlir::dyn_cast<mlir::ViewLikeOpInterface>(user)) {
      if (canResultEscape(user, false)) {
        return true;
      } else {
        continue;
      }
    }

    return true;
  }

  return false;
}

struct HoistBufferAllocs
    : public mlir::OpRewritePattern<mlir::memref::AllocOp> {
  using OpRewritePattern::OpRewritePattern;

  mlir::LogicalResult
  matchAndRewrite(mlir::memref::AllocOp op,
                  mlir::PatternRewriter &rewriter) const override {
    if (!op.getSymbolOperands().empty())
      return mlir::failure();

    if (canResultEscape(op))
      return mlir::failure();

    auto loopInfo = getLoopInfo(op);
    if (!loopInfo)
      return mlir::failure();

    auto func = op->getParentOfType<mlir::func::FuncOp>();
    if (!func)
      return mlir::failure();

    auto mc = func->getAttrOfType<mlir::IntegerAttr>(
        numba::util::attributes::getMaxConcurrencyName());
    if (loopInfo->innermostParallel && !mc)
      return mlir::failure();

    bool needParallel =
        loopInfo->innermostParallel && mc.getValue().getSExtValue() > 0;

    auto oldType = op.getType().cast<mlir::MemRefType>();
    auto memrefType = [&]() -> mlir::MemRefType {
      if (needParallel) {
        llvm::SmallVector<int64_t> newShape;
        newShape.emplace_back(mc.getValue().getSExtValue());
        auto oldShape = oldType.getShape();
        newShape.append(oldShape.begin(), oldShape.end());
        return mlir::MemRefType::get(newShape, oldType.getElementType(),
                                     mlir::MemRefLayoutAttrInterface{},
                                     oldType.getMemorySpace());
      }
      return oldType;
    }();

    for (auto user : llvm::make_early_inc_range(op->getUsers()))
      if (mlir::isa<mlir::memref::DeallocOp>(user))
        rewriter.eraseOp(user);

    auto loc = op.getLoc();
    mlir::OpBuilder::InsertionGuard g(rewriter);
    rewriter.setInsertionPoint(loopInfo->outermostLoop);
    mlir::Value newMemref = rewriter.create<mlir::memref::AllocOp>(
        loc, memrefType, op.getDynamicSizes(), op.getAlignmentAttr());
    mlir::Value view = newMemref;
    if (needParallel) {
      auto zero = rewriter.getIndexAttr(0);
      auto one = rewriter.getIndexAttr(1);

      auto rank = static_cast<unsigned>(memrefType.getRank());
      rewriter.setInsertionPointToStart(
          loopInfo->innermostParallel.getBodyBlock());
      llvm::SmallVector<mlir::OpFoldResult> offsets(rank, zero);
      llvm::SmallVector<mlir::OpFoldResult> sizes(rank, one);
      llvm::SmallVector<mlir::OpFoldResult> strides(rank, one);

      auto threadIndex = loopInfo->innermostParallel.getBodyThreadIndex();
      offsets[0] = threadIndex;
      for (auto i : llvm::seq(0u, rank - 1)) {
        auto val =
            rewriter.createOrFold<mlir::memref::DimOp>(loc, newMemref, i + 1);
        if (auto fixed = mlir::getConstantIntValue(val)) {
          sizes[i + 1] = rewriter.getIndexAttr(*fixed);
        } else {
          sizes[i + 1] = val;
        }
      }

      auto newType =
          mlir::memref::SubViewOp::inferRankReducedResultType(
              oldType.getShape(), memrefType, offsets, sizes, strides)
              .cast<mlir::MemRefType>();
      view = rewriter.create<mlir::memref::SubViewOp>(loc, newType, newMemref,
                                                      offsets, sizes, strides);

      if (view.getType() != oldType) {
        view = rewriter.create<numba::util::MemrefApplyOffsetOp>(loc, oldType,
                                                                 view);
        view = rewriter.create<mlir::memref::CastOp>(loc, oldType, view);
      }
    }

    rewriter.replaceOp(op, view);

    rewriter.setInsertionPointAfter(loopInfo->outermostLoop);
    rewriter.create<mlir::memref::DeallocOp>(loc, newMemref);
    return mlir::success();
  }
};

struct ParallelToTbbPass
    : public numba::RewriteWrapperPass<
          ParallelToTbbPass, mlir::func::FuncOp,
          numba::DependentDialectsList<numba::util::NumbaUtilDialect,
                                       mlir::arith::ArithDialect,
                                       mlir::scf::SCFDialect>,
          ParallelToTbb> {};

struct HoistBufferAllocsPass
    : public numba::RewriteWrapperPass<
          HoistBufferAllocsPass, mlir::func::FuncOp,
          numba::DependentDialectsList<numba::util::NumbaUtilDialect,
                                       mlir::scf::SCFDialect,
                                       mlir::memref::MemRefDialect>,
          HoistBufferAllocs> {};

static void populateParallelToTbbPipeline(mlir::OpPassManager &pm) {
  pm.addNestedPass<mlir::func::FuncOp>(
      mlir::createLoopInvariantCodeMotionPass());
  pm.addNestedPass<mlir::func::FuncOp>(
      std::make_unique<HoistBufferAllocsPass>());
  pm.addNestedPass<mlir::func::FuncOp>(mlir::createCanonicalizerPass());
  pm.addNestedPass<mlir::func::FuncOp>(mlir::createCSEPass());
  pm.addNestedPass<mlir::func::FuncOp>(mlir::createCanonicalizerPass());
}
} // namespace

void registerParallelToTBBPipeline(numba::PipelineRegistry &registry) {
  registry.registerPipeline([](auto sink) {
    auto stage = getLowerLoweringStage();
    auto llvm_pipeline = lowerToLLVMPipelineName();
    sink(parallelToTBBPipelineName(), {stage.begin}, {llvm_pipeline}, {},
         &populateParallelToTbbPipeline);
  });
}

llvm::StringRef parallelToTBBPipelineName() { return "parallel_to_tbb"; }
