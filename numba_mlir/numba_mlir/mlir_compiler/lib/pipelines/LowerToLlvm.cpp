// SPDX-FileCopyrightText: 2021 - 2022 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "pipelines/LowerToLlvm.hpp"

#include <mlir/Conversion/AffineToStandard/AffineToStandard.h>
#include <mlir/Conversion/ArithToLLVM/ArithToLLVM.h>
#include <mlir/Conversion/ComplexToLLVM/ComplexToLLVM.h>
#include <mlir/Conversion/ComplexToStandard/ComplexToStandard.h>
#include <mlir/Conversion/ControlFlowToLLVM/ControlFlowToLLVM.h>
#include <mlir/Conversion/FuncToLLVM/ConvertFuncToLLVM.h>
#include <mlir/Conversion/LLVMCommon/ConversionTarget.h>
#include <mlir/Conversion/LLVMCommon/LoweringOptions.h>
#include <mlir/Conversion/LLVMCommon/TypeConverter.h>
#include <mlir/Conversion/MathToLLVM/MathToLLVM.h>
#include <mlir/Conversion/MathToLibm/MathToLibm.h>
#include <mlir/Conversion/MemRefToLLVM/AllocLikeConversion.h>
#include <mlir/Conversion/MemRefToLLVM/MemRefToLLVM.h>
#include <mlir/Conversion/SCFToControlFlow/SCFToControlFlow.h>
#include <mlir/Conversion/UBToLLVM/UBToLLVM.h>
#include <mlir/Conversion/VectorToLLVM/ConvertVectorToLLVM.h>
#include <mlir/Dialect/Arith/IR/Arith.h>
#include <mlir/Dialect/Arith/Transforms/Passes.h>
#include <mlir/Dialect/ControlFlow/IR/ControlFlowOps.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/Dialect/GPU/IR/GPUDialect.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/MemRef/IR/MemRef.h>
#include <mlir/Dialect/MemRef/Transforms/Passes.h>
#include <mlir/Dialect/SCF/IR/SCF.h>
#include <mlir/Dialect/UB/IR/UBOps.h>
#include <mlir/Dialect/Vector/IR/VectorOps.h>
#include <mlir/Dialect/Vector/Transforms/LoweringPatterns.h>
#include <mlir/Dialect/Vector/Transforms/VectorRewritePatterns.h>
#include <mlir/Dialect/Vector/Transforms/VectorTransforms.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/IRMapping.h>
#include <mlir/IR/PatternMatch.h>
#include <mlir/Pass/Pass.h>
#include <mlir/Pass/PassManager.h>
#include <mlir/Transforms/GreedyPatternRewriteDriver.h>
#include <mlir/Transforms/Passes.h>

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/TypeSwitch.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Host.h>

#include "BasePipeline.hpp"

#include "numba/Compiler/PipelineRegistry.hpp"
#include "numba/Conversion/MathExtToLibm.hpp"
#include "numba/Conversion/UtilToLlvm.hpp"
#include "numba/Dialect/numba_util/Dialect.hpp"
#include "numba/Transforms/FuncUtils.hpp"
#include "numba/Transforms/RewriteWrapper.hpp"
#include "numba/Utils.hpp"

static const bool defineMeminfoFuncs = true;
static const bool ensureUniqueAllocPtr = true;

namespace {
static mlir::LowerToLLVMOptions getLLVMOptions(mlir::MLIRContext &context) {
  static llvm::DataLayout dl = []() {
    llvm::InitializeAllTargets();
    auto triple = llvm::sys::getProcessTriple();
    std::string errStr;
    auto target = llvm::TargetRegistry::lookupTarget(triple, errStr);
    if (nullptr == target)
      numba::reportError(llvm::Twine("Unable to get target: ") + errStr);

    llvm::TargetOptions target_opts;
    std::unique_ptr<llvm::TargetMachine> machine(target->createTargetMachine(
        triple, llvm::sys::getHostCPUName(), "", target_opts, std::nullopt));
    return machine->createDataLayout();
  }();
  mlir::LowerToLLVMOptions opts(&context);
  opts.dataLayout = dl;
  opts.useBarePtrCallConv = false;
  opts.allocLowering = mlir::LowerToLLVMOptions::AllocLowering::None;
  return opts;
}

static mlir::Value doCast(mlir::OpBuilder &builder, mlir::Location loc,
                          mlir::Value src, mlir::Type dstType) {
  if (src.getType() == dstType)
    return src;

  return builder.create<mlir::UnrealizedConversionCastOp>(loc, dstType, src)
      .getResult(0);
}

static mlir::Type convertTupleTypes(mlir::MLIRContext &context,
                                    mlir::TypeConverter &converter,
                                    mlir::TypeRange types) {
  if (types.empty())
    return mlir::LLVM::LLVMStructType::getLiteral(&context, std::nullopt);

  auto unitupleType = [&]() -> mlir::Type {
    assert(!types.empty());
    auto elemType = types.front();
    auto tail = types.drop_front();
    if (llvm::all_of(tail, [&](auto t) { return t == elemType; }))
      return elemType;

    return nullptr;
  }();

  auto count = static_cast<unsigned>(types.size());
  if (unitupleType) {
    auto newType = converter.convertType(unitupleType);
    if (!newType)
      return {};
    return mlir::LLVM::LLVMArrayType::get(newType, count);
  }
  llvm::SmallVector<mlir::Type> newTypes;
  newTypes.reserve(count);
  for (auto type : types) {
    auto newType = converter.convertType(type);
    if (!newType)
      return {};

    newTypes.emplace_back(newType);
  }

  return mlir::LLVM::LLVMStructType::getLiteral(&context, newTypes);
}

static mlir::Type convertTuple(mlir::MLIRContext &context,
                               mlir::TypeConverter &converter,
                               mlir::TupleType tuple) {
  return convertTupleTypes(context, converter, tuple.getTypes());
}

static mlir::Type getLLVMPointerType(mlir::Type elemType) {
  assert(elemType);
  return mlir::LLVM::LLVMPointerType::get(elemType.getContext());
}

static void
populateToLLVMAdditionalTypeConversion(mlir::LLVMTypeConverter &converter) {
  converter.addConversion(
      [&converter](mlir::TupleType type) -> std::optional<mlir::Type> {
        auto res = convertTuple(*type.getContext(), converter, type);
        if (!res)
          return std::nullopt;
        return res;
      });
  auto voidPtrType =
      getLLVMPointerType(mlir::IntegerType::get(&converter.getContext(), 8));
  converter.addConversion(
      [voidPtrType](mlir::NoneType) -> std::optional<mlir::Type> {
        return voidPtrType;
      });
  converter.addConversion(
      [voidPtrType](numba::util::OpaqueType) -> std::optional<mlir::Type> {
        return voidPtrType;
      });
  converter.addConversion(
      [voidPtrType](numba::util::TypeVarType) -> std::optional<mlir::Type> {
        return voidPtrType;
      });
}

struct LLVMTypeHelper {
  LLVMTypeHelper(mlir::MLIRContext &ctx) : type_converter(&ctx) {
    populateToLLVMAdditionalTypeConversion(type_converter);
  }

  mlir::Type i(unsigned bits) {
    return mlir::IntegerType::get(&type_converter.getContext(), bits);
  }

  mlir::Type ptr(mlir::Type type) {
    assert(static_cast<bool>(type));
    auto llType = type_converter.convertType(type);
    assert(static_cast<bool>(llType));
    return getLLVMPointerType(llType);
  }

  mlir::MLIRContext &get_context() { return type_converter.getContext(); }

  mlir::LLVMTypeConverter &get_type_converter() { return type_converter; }

private:
  mlir::LLVMTypeConverter type_converter;
};

static mlir::Type getExceptInfoType(LLVMTypeHelper &type_helper) {
  mlir::Type elems[] = {
      type_helper.ptr(type_helper.i(8)),
      type_helper.i(32),
      type_helper.ptr(type_helper.i(8)),
  };
  return mlir::LLVM::LLVMStructType::getLiteral(&type_helper.get_context(),
                                                elems);
}

static mlir::LLVM::LLVMStructType getArrayType(mlir::TypeConverter &converter,
                                               mlir::MemRefType type) {
  assert(type);
  auto ctx = type.getContext();
  auto i8p = getLLVMPointerType(mlir::IntegerType::get(ctx, 8));
  auto i64 = mlir::IntegerType::get(ctx, 64);
  auto dataType = converter.convertType(type.getElementType());
  assert(dataType);
  if (type.getRank() > 0) {
    auto shapeType = mlir::LLVM::LLVMArrayType::get(
        i64, static_cast<unsigned>(type.getRank()));
    const mlir::Type members[] = {
        i8p,                          // 0, meminfo
        i8p,                          // 1, parent
        i64,                          // 2, nitems
        i64,                          // 3, itemsize
        getLLVMPointerType(dataType), // 4, data
        shapeType,                    // 5, shape
        shapeType,                    // 6, strides
    };
    return mlir::LLVM::LLVMStructType::getLiteral(ctx, members);
  } else {
    const mlir::Type members[] = {
        i8p,                          // 0, meminfo
        i8p,                          // 1, parent
        i64,                          // 2, nitems
        i64,                          // 3, itemsize
        getLLVMPointerType(dataType), // 4, data
    };
    return mlir::LLVM::LLVMStructType::getLiteral(ctx, members);
  }
}

static mlir::Value wrapAllocPtr(mlir::OpBuilder &builder, mlir::Location loc,
                                mlir::ModuleOp mod, mlir::Value allocPtr) {
  if (!ensureUniqueAllocPtr)
    return allocPtr;

  auto ptrType = mlir::LLVM::LLVMPointerType::get(builder.getContext());
  mlir::StringRef funcName = "nmrtCreateAllocToken";
  auto funcType = mlir::LLVM::LLVMFunctionType::get(ptrType, {});
  auto func = numba::getOrInserLLVMFunc(builder, mod, funcName, funcType);
  mlir::Value token =
      builder.create<mlir::LLVM::CallOp>(loc, func, std::nullopt).getResult();
  builder.create<mlir::LLVM::StoreOp>(loc, allocPtr, token);
  return token;
}

static mlir::Value unwrapAllocPtr(mlir::OpBuilder &builder, mlir::Location loc,
                                  mlir::Value allocPtr) {
  if (!ensureUniqueAllocPtr)
    return allocPtr;

  auto ptrType = mlir::LLVM::LLVMPointerType::get(builder.getContext());
  return builder.create<mlir::LLVM::LoadOp>(loc, ptrType, allocPtr).getResult();
}

static void freeAllocPtrWrapper(mlir::OpBuilder &builder, mlir::Location loc,
                                mlir::ModuleOp mod, mlir::Value allocPtr) {
  if (!ensureUniqueAllocPtr)
    return;

  auto ptrType = mlir::LLVM::LLVMPointerType::get(builder.getContext());
  mlir::StringRef funcName = "nmrtDestroyAllocToken";
  auto voidType = mlir::LLVM::LLVMVoidType::get(builder.getContext());
  auto funcType = mlir::LLVM::LLVMFunctionType::get(voidType, ptrType);
  auto func = numba::getOrInserLLVMFunc(builder, mod, funcName, funcType);
  builder.create<mlir::LLVM::CallOp>(loc, func, allocPtr);
}

template <typename F> static void flattenType(mlir::Type type, F &&func) {
  if (auto struct_type = type.dyn_cast<mlir::LLVM::LLVMStructType>()) {
    for (auto elem : struct_type.getBody())
      flattenType(elem, std::forward<F>(func));

  } else if (auto arr_type = type.dyn_cast<mlir::LLVM::LLVMArrayType>()) {
    auto elem = arr_type.getElementType();
    auto size = arr_type.getNumElements();
    for (unsigned i = 0; i < size; ++i)
      flattenType(elem, std::forward<F>(func));

  } else {
    func(type);
  }
}

template <typename F>
static mlir::Value unflatten(mlir::Type type, mlir::Location loc,
                             mlir::OpBuilder &builder, F &&nextFunc) {
  namespace mllvm = mlir::LLVM;
  if (auto structType = type.dyn_cast<mlir::LLVM::LLVMStructType>()) {
    mlir::Value val = builder.create<mllvm::UndefOp>(loc, structType);
    for (auto &&[i, elemType] : llvm::enumerate(structType.getBody())) {
      auto elemIndex = static_cast<int64_t>(i);
      auto elemVal =
          unflatten(elemType, loc, builder, std::forward<F>(nextFunc));
      val = builder.create<mlir::LLVM::InsertValueOp>(loc, val, elemVal,
                                                      elemIndex);
    }
    return val;
  } else if (auto arrType = type.dyn_cast<mlir::LLVM::LLVMArrayType>()) {
    auto elemType = arrType.getElementType();
    auto size = arrType.getNumElements();
    mlir::Value val = builder.create<mllvm::UndefOp>(loc, arrType);
    for (unsigned i = 0; i < size; ++i) {
      auto elemVal =
          unflatten(elemType, loc, builder, std::forward<F>(nextFunc));
      val = builder.create<mlir::LLVM::InsertValueOp>(loc, val, elemVal, i);
    }
    return val;
  } else {
    return nextFunc();
  }
}

static void writeMemrefDesc(llvm::raw_ostream &os,
                            mlir::MemRefType memrefType) {
  if (memrefType.hasRank()) {
    auto rank = memrefType.getRank();
    assert(rank >= 0);
    if (rank > 0)
      os << memrefType.getRank() << "x";

  } else {
    os << "?x";
  }
  memrefType.getElementType().print(os);
}

static std::string genToMemrefConversionFuncName(mlir::MemRefType memrefType) {
  assert(memrefType);
  std::string ret;
  llvm::raw_string_ostream ss(ret);
  ss << "__convert_to_memref_";
  writeMemrefDesc(ss, memrefType);
  ss.flush();
  return ret;
}

static std::string
genFromMemrefConversionFuncName(mlir::MemRefType memrefType) {
  assert(memrefType);
  std::string ret;
  llvm::raw_string_ostream ss(ret);
  ss << "__convert_from_memref_";
  writeMemrefDesc(ss, memrefType);
  ss.flush();
  return ret;
}

static mlir::Value divStrides(mlir::Location loc, mlir::OpBuilder &builder,
                              mlir::Value strides, mlir::Value m) {
  auto arrayType = strides.getType().cast<mlir::LLVM::LLVMArrayType>();
  mlir::Value array = builder.create<mlir::LLVM::UndefOp>(loc, arrayType);
  auto count = arrayType.getNumElements();
  for (auto i : llvm::seq(0u, count)) {
    mlir::Value prev = builder.create<mlir::LLVM::ExtractValueOp>(
        loc, arrayType.getElementType(), strides, i);
    mlir::Value val = builder.create<mlir::LLVM::SDivOp>(loc, prev, m);
    array = builder.create<mlir::LLVM::InsertValueOp>(loc, array, val, i);
  }
  return array;
}

static mlir::Value mulStrides(mlir::Location loc, mlir::OpBuilder &builder,
                              mlir::Value strides, mlir::Value m) {
  auto arrayType = strides.getType().cast<mlir::LLVM::LLVMArrayType>();
  mlir::Value array = builder.create<mlir::LLVM::UndefOp>(loc, arrayType);
  auto count = arrayType.getNumElements();
  for (auto i : llvm::seq(0u, count)) {
    mlir::Value prev = builder.create<mlir::LLVM::ExtractValueOp>(
        loc, arrayType.getElementType(), strides, i);
    mlir::Value val = builder.create<mlir::LLVM::MulOp>(loc, prev, m);
    array = builder.create<mlir::LLVM::InsertValueOp>(loc, array, val, i);
  }
  return array;
}

static unsigned itemSize(mlir::Type type) {
  if (auto inttype = type.dyn_cast<mlir::IntegerType>()) {
    assert((inttype.getWidth() % 8) == 0);
    return inttype.getWidth() / 8;
  }

  if (auto floattype = type.dyn_cast<mlir::FloatType>()) {
    assert((floattype.getWidth() % 8) == 0);
    return floattype.getWidth() / 8;
  }

  if (auto complexType = type.dyn_cast<mlir::ComplexType>())
    return itemSize(complexType.getElementType()) * 2;

  llvm_unreachable("item_size: invalid type");
}

static mlir::func::FuncOp
getToMemrefConversionFunc(mlir::ModuleOp module, mlir::OpBuilder &builder,
                          mlir::MemRefType memrefType,
                          mlir::LLVM::LLVMStructType srcType,
                          mlir::LLVM::LLVMStructType dstType) {
  assert(memrefType);
  assert(srcType);
  assert(dstType);
  auto funcName = genToMemrefConversionFuncName(memrefType);
  if (auto func = module.lookupSymbol<mlir::func::FuncOp>(funcName)) {
    assert(func.getFunctionType().getNumResults() == 1);
    assert(func.getFunctionType().getResult(0) == dstType);
    return func;
  }
  auto funcType =
      mlir::FunctionType::get(builder.getContext(), srcType, dstType);
  auto loc = builder.getUnknownLoc();
  auto newFunc = numba::addFunction(builder, module, funcName, funcType);
  auto alwaysinline =
      mlir::StringAttr::get(builder.getContext(), "alwaysinline");
  newFunc->setAttr("passthrough",
                   mlir::ArrayAttr::get(builder.getContext(), alwaysinline));
  mlir::OpBuilder::InsertionGuard guard(builder);
  auto block = newFunc.addEntryBlock();
  builder.setInsertionPointToStart(block);
  namespace mllvm = mlir::LLVM;
  mlir::Value arg = block->getArgument(0);
  auto extract = [&](unsigned index) -> mlir::Value {
    auto resType = srcType.getBody()[index];
    return builder.create<mllvm::ExtractValueOp>(loc, resType, arg, index);
  };
  auto meminfo = extract(0);
  auto ptr = extract(4);
  auto rank = memrefType.getRank();
  auto shape = (rank > 0 ? extract(5) : mlir::Value());
  auto strides = (rank > 0 ? extract(6) : mlir::Value());
  auto i64 = mlir::IntegerType::get(builder.getContext(), 64);
  auto offset =
      builder.create<mllvm::ConstantOp>(loc, i64, builder.getI64IntegerAttr(0));
  mlir::Value res = builder.create<mllvm::UndefOp>(loc, dstType);
  meminfo = wrapAllocPtr(builder, loc, module, meminfo);
  auto itemsize = builder.create<mllvm::ConstantOp>(
      loc, i64,
      builder.getI64IntegerAttr(itemSize(memrefType.getElementType())));
  auto insert = [&](unsigned index, mlir::Value val) {
    res = builder.create<mllvm::InsertValueOp>(loc, res, val, index);
  };
  insert(0, meminfo);
  insert(1, ptr);
  insert(2, offset);
  if (rank > 0) {
    insert(3, shape);
    insert(4, divStrides(loc, builder, strides, itemsize));
  }
  builder.create<mllvm::ReturnOp>(loc, res);
  return newFunc;
}

static mlir::func::FuncOp
getFromMemrefConversionFunc(mlir::ModuleOp module, mlir::OpBuilder &builder,
                            mlir::MemRefType memrefType, mlir::Type elemType,
                            mlir::LLVM::LLVMStructType srcType,
                            mlir::LLVM::LLVMStructType dstType) {
  assert(memrefType);
  assert(srcType);
  assert(dstType);
  auto funcName = genFromMemrefConversionFuncName(memrefType);
  if (auto func = module.lookupSymbol<mlir::func::FuncOp>(funcName)) {
    assert(func.getFunctionType().getNumResults() == 1);
    assert(func.getFunctionType().getResult(0) == dstType);
    return func;
  }
  auto funcType =
      mlir::FunctionType::get(builder.getContext(), srcType, dstType);
  auto loc = builder.getUnknownLoc();
  auto newFunc = numba::addFunction(builder, module, funcName, funcType);
  auto alwaysinline =
      mlir::StringAttr::get(builder.getContext(), "alwaysinline");
  newFunc->setAttr("passthrough",
                   mlir::ArrayAttr::get(builder.getContext(), alwaysinline));
  mlir::OpBuilder::InsertionGuard guard(builder);
  auto block = newFunc.addEntryBlock();
  builder.setInsertionPointToStart(block);
  namespace mllvm = mlir::LLVM;
  mlir::Value arg = block->getArgument(0);
  auto i8ptrType = getLLVMPointerType(builder.getIntegerType(8));
  auto i64Type = builder.getIntegerType(64);
  auto extract = [&](unsigned index) -> mlir::Value {
    auto resType = srcType.getBody()[index];
    return builder.create<mllvm::ExtractValueOp>(loc, resType, arg, index);
  };
  auto allocPtr = extract(0);
  auto origPtr = extract(1);
  auto offset = extract(2);
  auto rank = memrefType.getRank();
  auto shape = (rank > 0 ? extract(3) : mlir::Value());
  auto strides = (rank > 0 ? extract(4) : mlir::Value());

  auto meminfo = unwrapAllocPtr(builder, loc, allocPtr);
  freeAllocPtrWrapper(builder, loc, module, allocPtr);
  auto ptr = builder.create<mllvm::GEPOp>(loc, origPtr.getType(), elemType,
                                          origPtr, offset);
  mlir::Value res = builder.create<mllvm::UndefOp>(loc, dstType);
  auto null = builder.create<mllvm::ZeroOp>(loc, i8ptrType);
  mlir::Value nitems = builder.create<mllvm::ConstantOp>(
      loc, i64Type, builder.getI64IntegerAttr(1));
  for (int64_t i = 0; i < rank; ++i) {
    auto dim =
        builder.create<mllvm::ExtractValueOp>(loc, nitems.getType(), shape, i);
    nitems = builder.create<mllvm::MulOp>(loc, nitems, dim);
  }
  auto itemsize = builder.create<mllvm::ConstantOp>(
      loc, i64Type,
      builder.getI64IntegerAttr(itemSize(memrefType.getElementType())));
  auto insert = [&](unsigned index, mlir::Value val) {
    res = builder.create<mllvm::InsertValueOp>(loc, res, val, index);
  };
  insert(0, meminfo);
  insert(1, null); // parent
  insert(2, nitems);
  insert(3, itemsize);
  insert(4, ptr);
  if (rank > 0) {
    insert(5, shape);
    insert(6, mulStrides(loc, builder, strides, itemsize));
  }
  builder.create<mllvm::ReturnOp>(loc, res);
  return newFunc;
}

static mlir::Attribute getFastmathAttrs(mlir::MLIRContext &ctx) {
  auto addPair = [&](auto name, auto val) {
    const mlir::Attribute attrs[] = {mlir::StringAttr::get(&ctx, name),
                                     mlir::StringAttr::get(&ctx, val)};
    return mlir::ArrayAttr::get(&ctx, attrs);
  };
  const mlir::Attribute attrs[] = {
      addPair("denormal-fp-math", "preserve-sign,preserve-sign"),
      addPair("denormal-fp-math-f32", "ieee,ieee"),
      addPair("no-infs-fp-math", "true"),
      addPair("no-nans-fp-math", "true"),
      addPair("no-signed-zeros-fp-math", "true"),
      addPair("unsafe-fp-math", "true"),
      addPair(numba::util::attributes::getFastmathName(), "1"),
  };
  return mlir::ArrayAttr::get(&ctx, attrs);
}

static mlir::Type getFunctionResType(mlir::MLIRContext &context,
                                     mlir::TypeConverter &converter,
                                     mlir::TypeRange types) {
  if (types.empty())
    return getLLVMPointerType(mlir::IntegerType::get(&context, 8));

  llvm::SmallVector<mlir::Type> newResTypes(types.size());
  for (auto &&[i, type] : llvm::enumerate(types)) {
    if (auto memreftype = type.dyn_cast<mlir::MemRefType>()) {
      newResTypes[i] = getArrayType(converter, memreftype);
    } else {
      newResTypes[i] = type;
    }
  }

  if (newResTypes.size() == 1)
    return newResTypes.front();

  return convertTupleTypes(context, converter, newResTypes);
}

static mlir::LogicalResult fixFuncSig(LLVMTypeHelper &typeHelper,
                                      mlir::func::FuncOp func) {
  if (func.isPrivate())
    return mlir::success();

  if (func->getAttr(numba::util::attributes::getFastmathName()))
    func->setAttr("passthrough", getFastmathAttrs(*func.getContext()));

  auto oldType = func.getFunctionType();
  auto &ctx = *oldType.getContext();
  llvm::SmallVector<mlir::Type> args;

  auto ptr = [&](auto arg) { return typeHelper.ptr(arg); };

  mlir::OpBuilder builder(&ctx);
  auto uloc = builder.getUnknownLoc();
  unsigned index = 0;
  auto addArg = [&](mlir::Type type) {
    args.push_back(type);
    auto ret = func.getBody().insertArgument(index, type, uloc);
    ++index;
    return ret;
  };

  auto &typeConverter = typeHelper.get_type_converter();
  auto &context = typeConverter.getContext();
  auto origRetType =
      getFunctionResType(context, typeConverter, oldType.getResults());
  if (!origRetType)
    return mlir::failure();

  if (!typeConverter.convertType(origRetType)) {
    func->emitError("fixFuncSig: couldn't convert return type: ")
        << origRetType;
    return mlir::failure();
  }

  builder.setInsertionPointToStart(&func.getBody().front());

  auto loc = builder.getUnknownLoc();
  llvm::SmallVector<mlir::Value> newArgs;
  auto processArg = [&](mlir::Type type) {
    if (auto memrefType = type.dyn_cast<mlir::MemRefType>()) {
      newArgs.clear();
      auto arrType = getArrayType(typeConverter, memrefType);
      flattenType(arrType, [&](mlir::Type new_type) {
        newArgs.push_back(addArg(new_type));
      });
      auto it = newArgs.begin();
      mlir::Value desc = unflatten(arrType, loc, builder, [&]() {
        auto ret = *it;
        ++it;
        return ret;
      });

      auto mod = mlir::cast<mlir::ModuleOp>(func->getParentOp());
      auto dstType = typeConverter.convertType(memrefType);
      assert(dstType);
      auto convFunc =
          getToMemrefConversionFunc(mod, builder, memrefType, arrType,
                                    dstType.cast<mlir::LLVM::LLVMStructType>());
      auto converted =
          builder.create<mlir::func::CallOp>(loc, convFunc, desc).getResult(0);
      auto casted = doCast(builder, loc, converted, memrefType);
      func.getBody().getArgument(index).replaceAllUsesWith(casted);
      func.getBody().eraseArgument(index);
    } else {
      args.push_back(type);
      ++index;
    }
  };

  addArg(ptr(origRetType));
  addArg(ptr(ptr(getExceptInfoType(typeHelper))));

  auto oldArgs = oldType.getInputs();
  for (auto arg : oldArgs)
    processArg(arg);

  auto retType = mlir::IntegerType::get(&ctx, 32);
  func.setType(mlir::FunctionType::get(&ctx, args, retType));
  return mlir::success();
}

struct ReturnOpLowering : public mlir::OpRewritePattern<mlir::func::ReturnOp> {
  ReturnOpLowering(mlir::MLIRContext *ctx, mlir::TypeConverter &converter)
      : OpRewritePattern(ctx), typeConverter(converter) {}

  mlir::LogicalResult
  matchAndRewrite(mlir::func::ReturnOp op,
                  mlir::PatternRewriter &rewriter) const override {
    auto parent = op->getParentOfType<mlir::func::FuncOp>();
    if (nullptr == parent || parent.isPrivate())
      return mlir::failure();

    auto mod = op->getParentOfType<mlir::ModuleOp>();
    if (!mod)
      return mlir::failure();

    auto ctx = op.getContext();
    auto loc = op.getLoc();

    auto convertVal = [&](mlir::Value val) -> mlir::Value {
      auto origType = val.getType();
      auto llRetType = typeConverter.convertType(origType);
      if (!llRetType)
        return {};

      if (origType.isa<mlir::NoneType>())
        return rewriter.create<mlir::LLVM::ZeroOp>(loc, llRetType);

      val = doCast(rewriter, loc, val, llRetType);
      if (auto memrefType = origType.dyn_cast<mlir::MemRefType>()) {
        auto elemType = typeConverter.convertType(memrefType.getElementType());
        assert(elemType);

        auto dstType = getArrayType(typeConverter, memrefType)
                           .cast<mlir::LLVM::LLVMStructType>();
        auto func = getFromMemrefConversionFunc(
            mod, rewriter, memrefType, elemType,
            llRetType.cast<mlir::LLVM::LLVMStructType>(), dstType);
        val = rewriter.create<mlir::func::CallOp>(loc, func, val).getResult(0);
      }
      return val;
    };
    rewriter.setInsertionPoint(op);

    auto addr = op->getParentRegion()->front().getArgument(0);
    if (op.getNumOperands() == 0) {
      auto addrType = addr.getType();
      assert(addrType.isa<mlir::LLVM::LLVMPointerType>());
      auto llVal = rewriter.create<mlir::LLVM::ZeroOp>(loc, addrType);
      rewriter.create<mlir::LLVM::StoreOp>(loc, llVal, addr);
    } else if (op.getNumOperands() == 1) {
      mlir::Value val = convertVal(op.getOperand(0));
      if (!val)
        return mlir::failure();
      rewriter.create<mlir::LLVM::StoreOp>(loc, val, addr);
    } else {
      auto resType =
          getFunctionResType(*ctx, typeConverter, op.getOperandTypes());
      mlir::Value val =
          rewriter.create<mlir::LLVM::UndefOp>(loc, resType).getResult();
      for (auto &&[i, funcArg] : llvm::enumerate(op.getOperands())) {
        auto arg = convertVal(funcArg);
        if (!arg)
          return mlir::failure();

        auto index = static_cast<int64_t>(i);
        val = rewriter.create<mlir::LLVM::InsertValueOp>(loc, val, arg, index);
      }
      rewriter.create<mlir::LLVM::StoreOp>(loc, val, addr);
    }

    auto retType = mlir::IntegerType::get(ctx, 32);
    mlir::Value ret = rewriter.create<mlir::LLVM::ConstantOp>(
        loc, retType, mlir::IntegerAttr::get(retType, 0));
    rewriter.replaceOpWithNewOp<mlir::LLVM::ReturnOp>(op, ret);

    return mlir::success();
  }

private:
  mlir::TypeConverter &typeConverter;
};

template <typename Op>
struct ApplyFastmathFlags : public mlir::OpRewritePattern<Op> {
  using mlir::OpRewritePattern<Op>::OpRewritePattern;

  mlir::LogicalResult
  matchAndRewrite(Op op, mlir::PatternRewriter &rewriter) const override {
    auto parent = mlir::cast<mlir::LLVM::LLVMFuncOp>(op->getParentOp());
    bool changed = false;

    rewriter.startOpModification(op);
    auto fmf = op.getFastmathFlags();
    getFastmathFlags(parent, [&](auto flag) {
      if (!mlir::LLVM::bitEnumContainsAny(fmf, flag)) {
        fmf = fmf | flag;
        changed = true;
      }
    });
    if (changed) {
      op.setFastmathFlagsAttr(
          mlir::LLVM::FastmathFlagsAttr::get(op.getContext(), fmf));
      rewriter.finalizeOpModification(op);
    } else {
      rewriter.cancelOpModification(op);
    }

    return mlir::success(changed);
  }

private:
  template <typename F>
  static void getFastmathFlags(mlir::LLVM::LLVMFuncOp func, F &&sink) {
    if (func->hasAttr(numba::util::attributes::getFastmathName()))
      sink(mlir::LLVM::FastmathFlags::fast);
  }
};

enum {
  MeminfoRefcntIndex = 0,
  MeminfoDataIndex = 3,
};

static mlir::Type getMeminfoType(const mlir::LLVMTypeConverter &converter) {
  auto indexType = converter.getIndexType();
  auto *context = &converter.getContext();
  auto voidPtrType = getLLVMPointerType(mlir::IntegerType::get(context, 8));
  const mlir::Type members[] = {
      indexType,   // refcnt
      voidPtrType, // dtor
      voidPtrType, // dtor_info
      voidPtrType, // data
      indexType,   // size
      voidPtrType, // external_allocator
  };
  return mlir::LLVM::LLVMStructType::getLiteral(context, members);
}

struct LowerRetainOp
    : public mlir::ConvertOpToLLVMPattern<numba::util::RetainOp> {
  using ConvertOpToLLVMPattern::ConvertOpToLLVMPattern;

  mlir::LogicalResult
  matchAndRewrite(numba::util::RetainOp op, OpAdaptor adaptor,
                  mlir::ConversionPatternRewriter &rewriter) const override {
    auto arg = adaptor.getSource();
    if (!arg.getType().isa<mlir::LLVM::LLVMStructType>())
      return mlir::failure();

    auto mod = op->getParentOfType<mlir::ModuleOp>();
    assert(mod);
    auto increfFunc = getIncrefFunc(rewriter, mod);

    mlir::MemRefDescriptor source(arg);

    auto loc = op.getLoc();
    mlir::Value ptr = source.allocatedPtr(rewriter, loc);
    ptr = unwrapAllocPtr(rewriter, loc, ptr);
    rewriter.create<mlir::LLVM::CallOp>(loc, increfFunc, ptr);
    ptr = wrapAllocPtr(rewriter, loc, mod, ptr);
    source.setAllocatedPtr(rewriter, loc, ptr);
    rewriter.replaceOp(op, {source});

    return mlir::success();
  }

private:
  mlir::LLVM::LLVMFuncOp getIncrefFunc(mlir::OpBuilder &builder,
                                       mlir::ModuleOp mod) const {
    llvm::StringRef funcName("NRT_incref");
    auto func = mod.lookupSymbol<mlir::LLVM::LLVMFuncOp>(funcName);
    if (!func) {
      auto loc = builder.getUnknownLoc();
      mlir::OpBuilder::InsertionGuard g(builder);
      auto body = mod.getBody();
      builder.setInsertionPoint(body, body->end());
      auto llvmVoidType = getVoidType();
      auto llvmVoidPointerType = getVoidPtrType();
      func = builder.create<mlir::LLVM::LLVMFuncOp>(
          loc, funcName,
          mlir::LLVM::LLVMFunctionType::get(llvmVoidType, llvmVoidPointerType));
      if (defineMeminfoFuncs) {
        func.setPrivate();
        auto block = func.addEntryBlock();
        builder.setInsertionPointToStart(block);
        auto arg = block->getArgument(0);
        auto meminfoType = getMeminfoType(*getTypeConverter());
        auto meminfoPtrType = getLLVMPointerType(meminfoType);
        auto meminfo =
            builder.create<mlir::LLVM::BitcastOp>(loc, meminfoPtrType, arg);

        auto llvmI32Type = builder.getI32Type();

        auto indexType = getIndexType();
        auto refcntType = getLLVMPointerType(indexType);
        auto i32zero = builder.create<mlir::LLVM::ConstantOp>(
            loc, llvmI32Type, builder.getI32IntegerAttr(0));
        auto refcntOffset = builder.create<mlir::LLVM::ConstantOp>(
            loc, llvmI32Type, builder.getI32IntegerAttr(MeminfoRefcntIndex));
        mlir::Value indices[] = {i32zero, refcntOffset};
        auto refcntPtr = builder.create<mlir::LLVM::GEPOp>(
            loc, refcntType, meminfoType, meminfo, indices);

        auto one = builder.create<mlir::LLVM::ConstantOp>(
            loc, indexType, builder.getIntegerAttr(indexType, 1));
        builder.create<mlir::LLVM::AtomicRMWOp>(
            loc, mlir::LLVM::AtomicBinOp::add, refcntPtr, one,
            mlir::LLVM::AtomicOrdering::seq_cst);
        builder.create<mlir::func::ReturnOp>(loc);
      }
    }
    return func;
  }
};

static mlir::LLVM::LLVMFuncOp
getAllocMemInfoFunc(mlir::OpBuilder &builder,
                    const mlir::LLVMTypeConverter &converter,
                    mlir::ModuleOp mod) {
  mlir::StringRef funcName = "nmrtAllocMemInfo";
  auto func = mod.lookupSymbol<mlir::LLVM::LLVMFuncOp>(funcName);
  if (func)
    return func;

  auto loc = builder.getUnknownLoc();
  auto ptr = mlir::LLVM::LLVMPointerType::get(builder.getContext());
  auto index = converter.getIndexType();
  auto funcType =
      mlir::LLVM::LLVMFunctionType::get(ptr, {ptr, index, ptr, ptr});

  mlir::OpBuilder::InsertionGuard g(builder);
  auto body = mod.getBody();
  builder.setInsertionPoint(body, body->end());
  return builder.create<mlir::LLVM::LLVMFuncOp>(loc, funcName, funcType);
}

struct LowerWrapAllocPointerOp
    : public mlir::ConvertOpToLLVMPattern<numba::util::WrapAllocatedPointer> {
  using ConvertOpToLLVMPattern::ConvertOpToLLVMPattern;

  mlir::LogicalResult
  matchAndRewrite(numba::util::WrapAllocatedPointer op, OpAdaptor adaptor,
                  mlir::ConversionPatternRewriter &rewriter) const override {
    auto mod = op->getParentOfType<mlir::ModuleOp>();
    if (!mod)
      return rewriter.notifyMatchFailure(op, "Top level op is not ModuleOp");

    auto dtorRef = adaptor.getDtor();
    auto deallocFunc = mod.lookupSymbol<mlir::LLVM::LLVMFuncOp>(dtorRef);
    if (!deallocFunc)
      return rewriter.notifyMatchFailure(op, [&](mlir::Diagnostic &diag) {
        diag << "Dealloc function not found " << dtorRef;
      });

    auto &converter = *getTypeConverter();
    auto allocMeminfoFunc = getAllocMemInfoFunc(rewriter, converter, mod);
    auto ptrType = mlir::LLVM::LLVMPointerType::get(rewriter.getContext());
    auto indexType = converter.getIndexType();

    auto wrapper = [&]() {
      auto voidType = mlir::LLVM::LLVMVoidType::get(rewriter.getContext());
      // Keep in sync with PythonRt.cpp MemInfoDtorFunction decl.
      auto wrapperFuncType = mlir::LLVM::LLVMFunctionType::get(
          voidType, {ptrType, indexType, ptrType});
      auto wrapperName =
          numba::getUniqueLLVMGlobalName(mod, dtorRef + "_wrapper");
      mlir::OpBuilder::InsertionGuard g(rewriter);
      auto loc = deallocFunc.getLoc();
      auto body = mod.getBody();
      rewriter.setInsertionPoint(body, body->end());
      auto func = rewriter.create<mlir::LLVM::LLVMFuncOp>(loc, wrapperName,
                                                          wrapperFuncType);
      func.setPrivate();
      auto block = func.addEntryBlock();
      rewriter.setInsertionPointToStart(block);
      auto ptr = block->getArgument(0);
      auto dtorData = block->getArgument(2);
      rewriter.create<mlir::LLVM::CallOp>(loc, deallocFunc,
                                          mlir::ValueRange({dtorData, ptr}));
      rewriter.create<mlir::LLVM::ReturnOp>(loc, std::nullopt);
      return func;
    }();

    auto loc = op.getLoc();
    mlir::Value wrapperPtr =
        rewriter.create<mlir::LLVM::AddressOfOp>(loc, wrapper);
    wrapperPtr =
        rewriter.create<mlir::LLVM::BitcastOp>(loc, ptrType, wrapperPtr);
    auto size = rewriter.create<mlir::LLVM::ConstantOp>(loc, indexType, 0);
    mlir::Value args[] = {
        adaptor.getPtr(),
        size,
        wrapperPtr,
        adaptor.getDtorData(),
    };
    mlir::Value res =
        rewriter.create<mlir::LLVM::CallOp>(loc, allocMeminfoFunc, args)
            .getResult();
    res = wrapAllocPtr(rewriter, loc, mod, res);
    rewriter.replaceOp(op, res);
    return mlir::success();
  }
};

struct LowerGetAllocToken
    : public mlir::ConvertOpToLLVMPattern<numba::util::GetAllocTokenOp> {
  using ConvertOpToLLVMPattern::ConvertOpToLLVMPattern;

  mlir::LogicalResult
  matchAndRewrite(numba::util::GetAllocTokenOp op, OpAdaptor adaptor,
                  mlir::ConversionPatternRewriter &rewriter) const override {
    mlir::MemRefDescriptor source(adaptor.getSource());

    auto loc = op.getLoc();
    auto allocPtr = source.allocatedPtr(rewriter, loc);

    auto &converter = *getTypeConverter();
    auto indexType = converter.getIndexType();
    mlir::Value res =
        rewriter.create<mlir::LLVM::PtrToIntOp>(loc, indexType, allocPtr);
    rewriter.replaceOp(op, res);
    return mlir::success();
  }
};

/// Try to match the kind of a memref.atomic_rmw to determine whether to use a
/// lowering to llvm.atomicrmw or fallback to llvm.cmpxchg.
static std::optional<mlir::LLVM::AtomicBinOp>
matchSimpleAtomicOp(mlir::memref::AtomicRMWOp atomicOp) {
  using namespace mlir;
  switch (atomicOp.getKind()) {
  case arith::AtomicRMWKind::addf:
    return LLVM::AtomicBinOp::fadd;
  case arith::AtomicRMWKind::addi:
    return LLVM::AtomicBinOp::add;
  case arith::AtomicRMWKind::assign:
    return LLVM::AtomicBinOp::xchg;
  case arith::AtomicRMWKind::maxs:
    return LLVM::AtomicBinOp::max;
  case arith::AtomicRMWKind::maxu:
    return LLVM::AtomicBinOp::umax;
  case arith::AtomicRMWKind::mins:
    return LLVM::AtomicBinOp::min;
  case arith::AtomicRMWKind::minu:
    return LLVM::AtomicBinOp::umin;
  case arith::AtomicRMWKind::ori:
    return LLVM::AtomicBinOp::_or;
  case arith::AtomicRMWKind::andi:
    return LLVM::AtomicBinOp::_and;
  default:
    return std::nullopt;
  }
  llvm_unreachable("Invalid AtomicRMWKind");
}

// TODO: upstream fixes
struct AtomicRMWOpLowering
    : public mlir::ConvertOpToLLVMPattern<mlir::memref::AtomicRMWOp> {
  using ConvertOpToLLVMPattern::ConvertOpToLLVMPattern;

  mlir::LogicalResult
  matchAndRewrite(mlir::memref::AtomicRMWOp atomicOp,
                  mlir::memref::AtomicRMWOp::Adaptor adaptor,
                  mlir::ConversionPatternRewriter &rewriter) const override {
    auto maybeKind = matchSimpleAtomicOp(atomicOp);
    if (!maybeKind)
      return mlir::failure();
    auto memRefType = atomicOp.getMemRefType();
    auto dataPtr =
        getStridedElementPtr(atomicOp.getLoc(), memRefType, adaptor.getMemref(),
                             adaptor.getIndices(), rewriter);
    rewriter.replaceOpWithNewOp<mlir::LLVM::AtomicRMWOp>(
        atomicOp, *maybeKind, dataPtr, adaptor.getValue(),
        mlir::LLVM::AtomicOrdering::acq_rel);
    return mlir::success();
  }
};

// TODO: upstream
struct LowerPoison : public mlir::ConvertOpToLLVMPattern<mlir::ub::PoisonOp> {
  using ConvertOpToLLVMPattern::ConvertOpToLLVMPattern;

  mlir::LogicalResult
  matchAndRewrite(mlir::ub::PoisonOp op,
                  mlir::ub::PoisonOp::Adaptor /*adaptor*/,
                  mlir::ConversionPatternRewriter &rewriter) const override {
    auto srcType = op.getType();
    if (!mlir::isa<mlir::MemRefType, mlir::NoneType>(srcType))
      return mlir::failure();

    auto converter = getTypeConverter();
    auto type = converter->convertType(srcType);
    if (!type)
      return mlir::failure();

    rewriter.replaceOpWithNewOp<mlir::LLVM::PoisonOp>(op, type);
    return mlir::success();
  }
};

struct AllocOpLowering : public mlir::AllocLikeOpLLVMLowering {
  AllocOpLowering(mlir::LLVMTypeConverter &converter)
      : AllocLikeOpLLVMLowering(mlir::memref::AllocOp::getOperationName(),
                                converter) {}

  std::tuple<mlir::Value, mlir::Value>
  allocateBuffer(mlir::ConversionPatternRewriter &rewriter, mlir::Location loc,
                 mlir::Value sizeBytes, mlir::Operation *op) const override {
    auto allocOp = mlir::cast<mlir::memref::AllocOp>(op);
    auto memRefType = allocOp.getType();
    mlir::Value alignment;
    if (auto alignmentAttr = allocOp.getAlignment()) {
      alignment = createIndexConstant(rewriter, loc, *alignmentAttr);
    } else if (!memRefType.getElementType().isSignlessIntOrIndexOrFloat()) {
      // In the case where no alignment is specified, we may want to override
      // `malloc's` behavior. `malloc` typically aligns at the size of the
      // biggest scalar on a target HW. For non-scalars, use the natural
      // alignment of the LLVM type given by the LLVM DataLayout.
      alignment = getSizeInBytes(loc, memRefType.getElementType(), rewriter);
    } else {
      alignment = createIndexConstant(
          rewriter, loc, 32 /*item_size(memRefType.getElementType())*/);
    }
    alignment = rewriter.create<mlir::LLVM::TruncOp>(
        loc, rewriter.getIntegerType(32), alignment);

    auto mod = allocOp->getParentOfType<mlir::ModuleOp>();
    auto allocPtr =
        createAllocCall(loc, "NRT_MemInfo_alloc_safe_aligned", getVoidPtrType(),
                        {sizeBytes, alignment}, mod, rewriter);
    auto dataPtr = getDataPtr(loc, rewriter, allocPtr);

    allocPtr = wrapAllocPtr(rewriter, loc, mod, allocPtr);
    return std::make_tuple(allocPtr, dataPtr);
  }

private:
  mlir::Value createIndexConstant(mlir::OpBuilder &builder, mlir::Location loc,
                                  int64_t val) const {
    return createIndexAttrConstant(builder, loc, getIndexType(), val);
  }

  mlir::Value createAllocCall(mlir::Location loc, mlir::StringRef name,
                              mlir::Type ptrType,
                              mlir::ArrayRef<mlir::Value> params,
                              mlir::ModuleOp module,
                              mlir::ConversionPatternRewriter &rewriter) const {
    using namespace mlir;
    SmallVector<Type, 2> paramTypes;
    auto allocFuncOp = module.lookupSymbol<LLVM::LLVMFuncOp>(name);
    if (!allocFuncOp) {
      for (Value param : params)
        paramTypes.push_back(param.getType());
      auto allocFuncType =
          LLVM::LLVMFunctionType::get(getVoidPtrType(), paramTypes);
      OpBuilder::InsertionGuard guard(rewriter);
      auto body = module.getBody();
      rewriter.setInsertionPoint(body, body->end());
      allocFuncOp = rewriter.create<LLVM::LLVMFuncOp>(rewriter.getUnknownLoc(),
                                                      name, allocFuncType);
    }

    auto allocFuncSymbol = mlir::SymbolRefAttr::get(allocFuncOp);
    auto allocatedPtr = rewriter
                            .create<LLVM::CallOp>(loc, getVoidPtrType(),
                                                  allocFuncSymbol, params)
                            .getResult();
    return rewriter.create<LLVM::BitcastOp>(loc, ptrType, allocatedPtr);
  }

  mlir::Value getDataPtr(mlir::Location loc,
                         mlir::ConversionPatternRewriter &rewriter,
                         mlir::Value allocPtr) const {
    auto meminfoType = getMeminfoType(*getTypeConverter());
    auto meminfoPtrType = getLLVMPointerType(meminfoType);
    auto meminfo =
        rewriter.create<mlir::LLVM::BitcastOp>(loc, meminfoPtrType, allocPtr);

    auto dataPtrPtrType = getLLVMPointerType(getVoidPtrType());
    auto llvmI32Type = rewriter.getI32Type();
    auto i32zero = rewriter.create<mlir::LLVM::ConstantOp>(
        loc, llvmI32Type, rewriter.getI32IntegerAttr(0));
    auto dataOffset = rewriter.create<mlir::LLVM::ConstantOp>(
        loc, llvmI32Type, rewriter.getI32IntegerAttr(MeminfoDataIndex));
    mlir::Value indices[] = {i32zero, dataOffset};
    auto dataPtrPtr = rewriter.create<mlir::LLVM::GEPOp>(
        loc, dataPtrPtrType, meminfoType, meminfo, indices);
    return rewriter.create<mlir::LLVM::LoadOp>(loc, getVoidPtrType(),
                                               dataPtrPtr);
  }
};

struct DeallocOpLowering
    : public mlir::ConvertOpToLLVMPattern<mlir::memref::DeallocOp> {
  using ConvertOpToLLVMPattern<mlir::memref::DeallocOp>::ConvertOpToLLVMPattern;

  mlir::LogicalResult
  matchAndRewrite(mlir::memref::DeallocOp op,
                  mlir::memref::DeallocOp::Adaptor adaptor,
                  mlir::ConversionPatternRewriter &rewriter) const override {
    auto mod = op->getParentOfType<mlir::ModuleOp>();
    assert(mod);
    auto freeFunc = getDecrefFunc(rewriter, mod);

    auto loc = op.getLoc();
    mlir::MemRefDescriptor memref(adaptor.getMemref());
    auto ptr = memref.allocatedPtr(rewriter, loc);
    auto unwrapped = unwrapAllocPtr(rewriter, loc, ptr);
    rewriter.replaceOpWithNewOp<mlir::LLVM::CallOp>(
        op, mlir::TypeRange(), mlir::SymbolRefAttr::get(freeFunc), unwrapped);
    freeAllocPtrWrapper(rewriter, loc, mod, ptr);
    return mlir::success();
  }

private:
  mlir::LLVM::LLVMFuncOp getDecrefFunc(mlir::OpBuilder &builder,
                                       mlir::ModuleOp mod) const {
    llvm::StringRef funcName("NRT_decref");
    auto func = mod.lookupSymbol<mlir::LLVM::LLVMFuncOp>(funcName);
    if (!func) {
      auto loc = builder.getUnknownLoc();
      mlir::OpBuilder::InsertionGuard g(builder);
      auto body = mod.getBody();
      builder.setInsertionPoint(body, body->end());
      auto llvmVoidType = getVoidType();
      auto llvmVoidPointerType = getVoidPtrType();
      func = builder.create<mlir::LLVM::LLVMFuncOp>(
          loc, funcName,
          mlir::LLVM::LLVMFunctionType::get(llvmVoidType, llvmVoidPointerType));
      if (defineMeminfoFuncs) {
        func.setPrivate();
        auto block = func.addEntryBlock();
        auto releaseBlock = func.addBlock();
        auto returnBlock = func.addBlock();

        builder.setInsertionPointToStart(block);
        auto arg = block->getArgument(0);
        auto meminfoType = getMeminfoType(*getTypeConverter());
        auto meminfoPtrType = getLLVMPointerType(meminfoType);
        mlir::Value meminfo =
            builder.create<mlir::LLVM::BitcastOp>(loc, meminfoPtrType, arg);

        auto llvmI32Type = builder.getI32Type();

        auto indexType = getIndexType();
        auto refcntType = getLLVMPointerType(indexType);
        auto i32zero = builder.create<mlir::LLVM::ConstantOp>(
            loc, llvmI32Type, builder.getI32IntegerAttr(0));
        auto refcntOffset = builder.create<mlir::LLVM::ConstantOp>(
            loc, llvmI32Type, builder.getI32IntegerAttr(MeminfoRefcntIndex));
        mlir::Value indices[] = {i32zero, refcntOffset};
        auto refcntPtr = builder.create<mlir::LLVM::GEPOp>(
            loc, refcntType, meminfoType, meminfo, indices);

        auto one = builder.create<mlir::LLVM::ConstantOp>(
            loc, indexType, builder.getIntegerAttr(indexType, 1));
        auto res = builder.create<mlir::LLVM::AtomicRMWOp>(
            loc, mlir::LLVM::AtomicBinOp::sub, refcntPtr, one,
            mlir::LLVM::AtomicOrdering::seq_cst);

        auto isRelease = builder.create<mlir::LLVM::ICmpOp>(
            loc, mlir::LLVM::ICmpPredicate::eq, res, one);
        builder.create<mlir::LLVM::CondBrOp>(loc, isRelease, releaseBlock,
                                             returnBlock);

        builder.setInsertionPointToStart(releaseBlock);
        llvm::StringRef dtorFuncName("NRT_MemInfo_call_dtor");
        auto dtorFunc = mod.lookupSymbol<mlir::LLVM::LLVMFuncOp>(dtorFuncName);
        if (!dtorFunc) {
          mlir::OpBuilder::InsertionGuard g1(builder);
          auto body = mod.getBody();
          builder.setInsertionPoint(body, body->end());
          dtorFunc = builder.create<mlir::LLVM::LLVMFuncOp>(
              loc, dtorFuncName,
              mlir::LLVM::LLVMFunctionType::get(llvmVoidType, meminfoPtrType));
        }
        builder.create<mlir::LLVM::CallOp>(loc, mlir::TypeRange(),
                                           mlir::SymbolRefAttr::get(dtorFunc),
                                           meminfo);
        builder.create<mlir::func::ReturnOp>(loc);

        builder.setInsertionPointToStart(returnBlock);
        builder.create<mlir::func::ReturnOp>(loc);
      }
    }
    return func;
  }
};

class LLVMFunctionPass : public mlir::OperationPass<mlir::LLVM::LLVMFuncOp> {
public:
  using OperationPass<mlir::LLVM::LLVMFuncOp>::OperationPass;

  /// The polymorphic API that runs the pass over the currently held function.
  virtual void runOnFunction() = 0;

  /// The polymorphic API that runs the pass over the currently held operation.
  void runOnOperation() final {
    if (!getFunction().isExternal())
      runOnFunction();
  }

  /// Return the current function being transformed.
  mlir::LLVM::LLVMFuncOp getFunction() { return this->getOperation(); }
};

static void copyAttrs(mlir::Operation *src, mlir::Operation *dst) {
  const mlir::StringRef attrs[] = {
      numba::util::attributes::getFastmathName(),
      numba::util::attributes::getMaxConcurrencyName(),
  };
  for (auto name : attrs)
    if (auto attr = src->getAttr(name))
      dst->setAttr(name, attr);
}

struct LowerParallel : public mlir::OpRewritePattern<numba::util::ParallelOp> {
  LowerParallel(mlir::MLIRContext *context)
      : OpRewritePattern(context), converter(context) {}

  mlir::LogicalResult
  matchAndRewrite(numba::util::ParallelOp op,
                  mlir::PatternRewriter &rewriter) const override {
    auto numLoops = op.getNumLoops();
    llvm::SmallVector<mlir::Value> contextVars;
    llvm::SmallVector<mlir::Operation *> contextConstants;
    llvm::DenseSet<mlir::Value> contextVarsSet;
    auto addContextVar = [&](mlir::Value value) {
      if (0 != contextVarsSet.count(value))
        return;

      contextVarsSet.insert(value);
      if (auto op = value.getDefiningOp()) {
        if (op->hasTrait<mlir::OpTrait::ConstantLike>()) {
          contextConstants.emplace_back(op);
          return;
        }
      }
      contextVars.emplace_back(value);
    };

    auto isDefinedInside = [&](mlir::Value value) {
      auto &thisRegion = op.getRegion();
      auto opRegion = value.getParentRegion();
      assert(nullptr != opRegion);
      do {
        if (opRegion == &thisRegion)
          return true;

        opRegion = opRegion->getParentRegion();
      } while (nullptr != opRegion);
      return false;
    };

    if (op->walk([&](mlir::Operation *inner) -> mlir::WalkResult {
            if (op != inner) {
              for (auto arg : inner->getOperands()) {
                if (!isDefinedInside(arg))
                  addContextVar(arg);
              }
            }
            return mlir::WalkResult::advance();
          }).wasInterrupted()) {
      return mlir::failure();
    }

    auto contextType = [&]() -> mlir::LLVM::LLVMStructType {
      llvm::SmallVector<mlir::Type> fields;
      fields.reserve(contextVars.size());
      for (auto var : contextVars) {
        auto type = converter.convertType(var.getType());
        if (!type)
          return {};

        fields.emplace_back(type);
      }
      return mlir::LLVM::LLVMStructType::getLiteral(op.getContext(), fields);
    }();

    if (!contextType)
      return mlir::failure();

    numba::AllocaInsertionPoint allocaInsertionPoint(op);

    auto contextPtrType = getLLVMPointerType(contextType);

    auto loc = op.getLoc();
    auto indexType = rewriter.getIndexType();
    auto llvmIndexType = converter.getIndexType();
    auto toLLVMIndex = [&](mlir::Value val) -> mlir::Value {
      if (val.getType() != llvmIndexType) {
        return rewriter
            .create<mlir::UnrealizedConversionCastOp>(loc, llvmIndexType, val)
            .getResult(0);
      }
      return val;
    };
    auto fromLLVMIndex = [&](mlir::Value val) -> mlir::Value {
      if (val.getType() != indexType)
        return doCast(rewriter, loc, val, indexType);

      return val;
    };
    auto llvmI32Type = mlir::IntegerType::get(op.getContext(), 32);
    auto zero = rewriter.create<mlir::LLVM::ConstantOp>(
        loc, llvmI32Type, rewriter.getI32IntegerAttr(0));
    auto context = allocaInsertionPoint.insert(rewriter, [&]() {
      auto one = rewriter.create<mlir::LLVM::ConstantOp>(
          loc, llvmI32Type, rewriter.getI32IntegerAttr(1));
      return rewriter.create<mlir::LLVM::AllocaOp>(loc, contextPtrType,
                                                   contextType, one, 0);
    });

    for (auto &&it : llvm::enumerate(contextVars)) {
      auto type = contextType.getBody()[it.index()];
      auto llvmVal = doCast(rewriter, loc, it.value(), type);
      auto i = rewriter.getI32IntegerAttr(static_cast<int32_t>(it.index()));
      mlir::Value indices[] = {
          zero, rewriter.create<mlir::LLVM::ConstantOp>(loc, llvmI32Type, i)};
      auto pointerType = getLLVMPointerType(type);
      auto ptr = rewriter.create<mlir::LLVM::GEPOp>(
          loc, pointerType, contextType, context, indices);
      rewriter.create<mlir::LLVM::StoreOp>(loc, llvmVal, ptr);
    }
    auto voidPtrType =
        getLLVMPointerType(mlir::IntegerType::get(op.getContext(), 8));
    auto contextAbstract =
        rewriter.create<mlir::LLVM::BitcastOp>(loc, voidPtrType, context);

    auto inputRangeType = [&]() {
      const mlir::Type members[] = {
          llvmIndexType, // lower_bound
          llvmIndexType, // upper_bound
          llvmIndexType, // step
      };
      return mlir::LLVM::LLVMStructType::getLiteral(op.getContext(), members);
    }();
    auto inputRangePtr = getLLVMPointerType(inputRangeType);
    auto rangeType = [&]() {
      const mlir::Type members[] = {
          llvmIndexType, // lower_bound
          llvmIndexType, // upper_bound
      };
      return mlir::LLVM::LLVMStructType::getLiteral(op.getContext(), members);
    }();
    auto rangePtr = getLLVMPointerType(rangeType);
    auto funcType = [&]() {
      const mlir::Type args[] = {
          rangePtr,   // bounds
          indexType,  // thread index
          voidPtrType // context
      };
      return mlir::FunctionType::get(op.getContext(), args, {});
    }();

    auto mod = op->getParentOfType<mlir::ModuleOp>();
    auto outlinedFunc = [&]() -> mlir::func::FuncOp {
      auto func = [&]() {
        auto parentFunc = op->getParentOfType<mlir::func::FuncOp>();
        assert(parentFunc);
        auto funcName = [&]() {
          auto oldName = parentFunc.getName();
          for (int i = 0;; ++i) {
            auto name =
                (0 == i ? (llvm::Twine(oldName) + "_outlined").str()
                        : (llvm::Twine(oldName) + "_outlined_" + llvm::Twine(i))
                              .str());
            if (!mod.lookupSymbol<mlir::func::FuncOp>(name)) {
              return name;
            }
          }
        }();

        auto func = numba::addFunction(rewriter, mod, funcName, funcType);
        copyAttrs(parentFunc, func);
        return func;
      }();
      mlir::IRMapping mapping;
      auto oldEntry = op.getBody();
      auto entry = func.addEntryBlock();
      auto loc = rewriter.getUnknownLoc();
      mlir::OpBuilder::InsertionGuard guard(rewriter);
      rewriter.setInsertionPointToStart(entry);
      for (unsigned i = 0; i < numLoops; ++i) {
        auto arg = entry->getArgument(0);
        const mlir::Value indices[] = {rewriter.create<mlir::LLVM::ConstantOp>(
            loc, llvmI32Type,
            rewriter.getI32IntegerAttr(static_cast<int32_t>(i)))};
        auto ptr = rewriter.create<mlir::LLVM::GEPOp>(loc, rangePtr, rangeType,
                                                      arg, indices);
        auto dims = rewriter.create<mlir::LLVM::LoadOp>(loc, rangeType, ptr);
        auto lower = rewriter.create<mlir::LLVM::ExtractValueOp>(
            loc, llvmIndexType, dims, 0);
        auto upper = rewriter.create<mlir::LLVM::ExtractValueOp>(
            loc, llvmIndexType, dims, 1);
        mapping.map(oldEntry->getArgument(i), fromLLVMIndex(lower));
        mapping.map(oldEntry->getArgument(i + numLoops), fromLLVMIndex(upper));
      }
      mapping.map(oldEntry->getArgument(2 * numLoops),
                  entry->getArgument(1)); // thread index
      for (auto arg : contextConstants)
        rewriter.clone(*arg, mapping);

      auto contextPtr = rewriter.create<mlir::LLVM::BitcastOp>(
          loc, contextPtrType, entry->getArgument(2));
      auto zero = rewriter.create<mlir::LLVM::ConstantOp>(
          loc, llvmI32Type, rewriter.getI32IntegerAttr(0));
      for (auto &&[index, oldVal] : llvm::enumerate(contextVars)) {
        const mlir::Value indices[] = {
            zero, rewriter.create<mlir::LLVM::ConstantOp>(
                      loc, llvmI32Type,
                      rewriter.getI32IntegerAttr(static_cast<int32_t>(index)))};
        auto elemType = contextType.getBody()[index];
        auto pointerType = getLLVMPointerType(elemType);
        auto ptr = rewriter.create<mlir::LLVM::GEPOp>(
            loc, pointerType, contextType, contextPtr, indices);
        auto llvmVal = rewriter.create<mlir::LLVM::LoadOp>(loc, elemType, ptr);
        auto val = doCast(rewriter, loc, llvmVal, oldVal.getType());
        mapping.map(oldVal, val);
      }
      op.getRegion().cloneInto(&func.getBody(), mapping);
      auto &origEntry = *std::next(func.getBody().begin());
      rewriter.create<mlir::cf::BranchOp>(loc, &origEntry);
      for (auto &block : func.getBody()) {
        if (auto term =
                mlir::dyn_cast<numba::util::YieldOp>(block.getTerminator())) {
          rewriter.eraseOp(term);
          rewriter.setInsertionPointToEnd(&block);
          rewriter.create<mlir::func::ReturnOp>(loc);
        }
      }
      return func;
    }();

    auto parallelFor = [&]() {
      auto funcName = "nmrtParallelFor";
      if (auto sym = mod.lookupSymbol<mlir::func::FuncOp>(funcName))
        return sym;

      const mlir::Type args[] = {
          inputRangePtr, // bounds
          indexType,     // num_loops
          funcType,      // func
          voidPtrType    // context
      };
      auto parallelFuncType =
          mlir::FunctionType::get(op.getContext(), args, {});
      return numba::addFunction(rewriter, mod, funcName, parallelFuncType);
    }();
    auto funcAddr = rewriter.create<mlir::func::ConstantOp>(
        loc, funcType, mlir::SymbolRefAttr::get(outlinedFunc));

    auto inputRanges = allocaInsertionPoint.insert(rewriter, [&]() {
      auto numLoopsAttr = rewriter.getIntegerAttr(llvmIndexType, numLoops);
      auto numLoopsVar = rewriter.create<mlir::LLVM::ConstantOp>(
          loc, llvmIndexType, numLoopsAttr);
      return rewriter.create<mlir::LLVM::AllocaOp>(
          loc, inputRangePtr, inputRangeType, numLoopsVar, 0);
    });
    for (unsigned i = 0; i < numLoops; ++i) {
      mlir::Value inputRange =
          rewriter.create<mlir::LLVM::UndefOp>(loc, inputRangeType);
      auto insert = [&](mlir::Value val, unsigned index) {
        inputRange = rewriter.create<mlir::LLVM::InsertValueOp>(loc, inputRange,
                                                                val, index);
      };
      insert(toLLVMIndex(op.getLowerBounds()[i]), 0);
      insert(toLLVMIndex(op.getUpperBounds()[i]), 1);
      insert(toLLVMIndex(op.getSteps()[i]), 2);
      const mlir::Value indices[] = {rewriter.create<mlir::LLVM::ConstantOp>(
          loc, llvmI32Type, rewriter.getI32IntegerAttr(static_cast<int>(i)))};
      auto ptr = rewriter.create<mlir::LLVM::GEPOp>(
          loc, inputRangePtr, inputRangeType, inputRanges, indices);
      rewriter.create<mlir::LLVM::StoreOp>(loc, inputRange, ptr);
    }

    auto numLoopsVar =
        rewriter.create<mlir::arith::ConstantIndexOp>(loc, numLoops);
    const mlir::Value pfArgs[] = {inputRanges, numLoopsVar, funcAddr,
                                  contextAbstract};
    rewriter.replaceOpWithNewOp<mlir::func::CallOp>(op, parallelFor, pfArgs);
    return mlir::success();
  }

private:
  mlir::LLVMTypeConverter converter;
};

struct RemoveParallelRegion
    : public mlir::OpRewritePattern<numba::util::EnvironmentRegionOp> {
  using OpRewritePattern::OpRewritePattern;

  mlir::LogicalResult
  matchAndRewrite(numba::util::EnvironmentRegionOp op,
                  mlir::PatternRewriter &rewriter) const override {
    if (!mlir::isa<numba::util::ParallelAttr>(op.getEnvironment()))
      return mlir::failure();

    numba::util::EnvironmentRegionOp::inlineIntoParent(rewriter, op);
    return mlir::success();
  }
};

struct RemoveParallelRegionPass
    : public numba::RewriteWrapperPass<RemoveParallelRegionPass, void, void,
                                       RemoveParallelRegion> {};

struct LowerParallelToCFGPass
    : public mlir::PassWrapper<LowerParallelToCFGPass,
                               mlir::OperationPass<void>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(LowerParallelToCFGPass)

  virtual void
  getDependentDialects(mlir::DialectRegistry &registry) const override {
    registry.insert<mlir::LLVM::LLVMDialect>();
    registry.insert<mlir::cf::ControlFlowDialect>();
    registry.insert<mlir::func::FuncDialect>();
  }

  void runOnOperation() override final {
    auto &context = getContext();
    mlir::RewritePatternSet patterns(&context);
    patterns.insert<LowerParallel>(&getContext());

    if (mlir::failed(mlir::applyPatternsAndFoldGreedily(getOperation(),
                                                        std::move(patterns))))
      return signalPassFailure();
  }
};

struct PreLLVMLowering
    : public mlir::PassWrapper<PreLLVMLowering,
                               mlir::OperationPass<mlir::func::FuncOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(PreLLVMLowering)

  virtual void
  getDependentDialects(mlir::DialectRegistry &registry) const override {
    registry.insert<mlir::LLVM::LLVMDialect>();
    registry.insert<mlir::func::FuncDialect>();
  }

  void runOnOperation() override final {
    auto &context = getContext();
    LLVMTypeHelper type_helper(context);

    mlir::RewritePatternSet patterns(&context);
    auto func = getOperation();

    if (mlir::failed(fixFuncSig(type_helper, func)))
      return signalPassFailure();

    patterns.insert<ReturnOpLowering>(&context,
                                      type_helper.get_type_converter());

    if (mlir::failed(mlir::applyPatternsAndFoldGreedily(getOperation(),
                                                        std::move(patterns))))
      return signalPassFailure();
  }
};

struct FixLLVMStructABIPass
    : public mlir::PassWrapper<FixLLVMStructABIPass,
                               mlir::OperationPass<mlir::ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(FixLLVMStructABIPass)

  virtual void
  getDependentDialects(mlir::DialectRegistry &registry) const override {
    registry.insert<mlir::LLVM::LLVMDialect>();
  }

  void runOnOperation() override final {
    // LLVM structs should always be passes as pointers to external calls

    auto mod = getOperation();

    mlir::OpBuilder builder(&getContext());

    auto unknownLoc = builder.getUnknownLoc();
    llvm::SmallVector<mlir::Type> newFuncTypes;
    llvm::SmallVector<mlir::Value> newArgs;
    mod->walk([&](mlir::LLVM::LLVMFuncOp func) -> mlir::WalkResult {
      if (!func.isExternal())
        return mlir::WalkResult::advance();

      auto funcType = func.getFunctionType();

      bool changed = false;
      newFuncTypes.clear();
      for (auto type : funcType.getParams()) {
        if (type.isa<mlir::LLVM::LLVMStructType>()) {
          changed = true;
          newFuncTypes.emplace_back(getLLVMPointerType(type));
        } else {
          newFuncTypes.emplace_back(type);
        }
      }

      if (!changed)
        return mlir::WalkResult::advance();

      auto newFuncType = mlir::LLVM::LLVMFunctionType::get(
          funcType.getReturnType(), newFuncTypes, funcType.isVarArg());
      func.setFunctionType(newFuncType);

      auto uses = mlir::SymbolTable::getSymbolUses(func, mod);
      if (!uses)
        return mlir::WalkResult::advance();

      for (auto use : *uses) {
        auto user = mlir::dyn_cast<mlir::LLVM::CallOp>(use.getUser());
        if (!user) {
          user->emitError("Unsupported functions user");
          signalPassFailure();
          return mlir::WalkResult::interrupt();
        }

        newArgs.clear();
        numba::AllocaInsertionPoint allocaHelper(user);
        allocaHelper.insert(builder, [&] {
          for (auto &&[arg, newType] :
               llvm::zip(user->getOperands(), newFuncTypes)) {
            auto origType = arg.getType();
            if (origType == newType) {
              newArgs.emplace_back(arg);
              continue;
            }

            auto one = builder.create<mlir::LLVM::ConstantOp>(
                unknownLoc, builder.getI32Type(), builder.getI32IntegerAttr(1));
            mlir::Value res = builder.create<mlir::LLVM::AllocaOp>(
                unknownLoc, newType, origType, one, 0);
            newArgs.emplace_back(res);
          }
        });
        auto loc = user.getLoc();
        builder.setInsertionPoint(user);
        for (auto &&[arg, newArg] : llvm::zip(user->getOperands(), newArgs)) {
          auto origType = arg.getType();
          auto newType = newArg.getType();
          if (origType == newType)
            continue;

          builder.create<mlir::LLVM::StoreOp>(loc, arg, newArg);
        }
        user->setOperands(newArgs);
      }

      return mlir::WalkResult::advance();
    });
  }
};

struct PostLLVMLowering
    : public mlir::PassWrapper<PostLLVMLowering, LLVMFunctionPass> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(PostLLVMLowering)

  virtual void
  getDependentDialects(mlir::DialectRegistry &registry) const override {
    registry.insert<mlir::LLVM::LLVMDialect>();
  }

  void runOnFunction() override final {
    auto &context = getContext();
    mlir::RewritePatternSet patterns(&context);

    patterns.insert<ApplyFastmathFlags<mlir::LLVM::FAddOp>,
                    ApplyFastmathFlags<mlir::LLVM::FSubOp>,
                    ApplyFastmathFlags<mlir::LLVM::FMulOp>,
                    ApplyFastmathFlags<mlir::LLVM::FDivOp>,
                    ApplyFastmathFlags<mlir::LLVM::FRemOp>,
                    ApplyFastmathFlags<mlir::LLVM::FCmpOp>,
                    ApplyFastmathFlags<mlir::LLVM::CallOp>>(&context);

    if (mlir::failed(mlir::applyPatternsAndFoldGreedily(getOperation(),
                                                        std::move(patterns))))
      return signalPassFailure();
  }
};

struct LowerVectorOps
    : public mlir::PassWrapper<LowerVectorOps, mlir::OperationPass<>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(LowerVectorOps)

  /// Run the dialect converter on the module.
  void runOnOperation() override {
    using namespace mlir::vector;
    mlir::RewritePatternSet patterns(&getContext());
    populateVectorToVectorCanonicalizationPatterns(patterns);
    populateVectorBroadcastLoweringPatterns(patterns);
    populateVectorContractLoweringPatterns(patterns, VectorTransformsOptions());
    populateVectorMaskOpLoweringPatterns(patterns);
    populateVectorShapeCastLoweringPatterns(patterns);
    populateVectorTransposeLoweringPatterns(patterns,
                                            VectorTransformsOptions());
    // Vector transfer ops with rank > 1 should be lowered with VectorToSCF.
    populateVectorTransferLoweringPatterns(patterns, /*maxTransferRank=*/1);
    if (mlir::failed(mlir::applyPatternsAndFoldGreedily(getOperation(),
                                                        std::move(patterns))))
      return signalPassFailure();
  }
};

// Copypasted from mlir
struct LLVMLoweringPass
    : public mlir::PassWrapper<LLVMLoweringPass,
                               mlir::OperationPass<mlir::ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(LLVMLoweringPass)

  /// Run the dialect converter on the module.
  void runOnOperation() override {
    using namespace mlir;
    auto &context = getContext();
    auto options = getLLVMOptions(context);
    if (failed(LLVM::LLVMDialect::verifyDataLayoutString(
            options.dataLayout.getStringRepresentation(),
            [this](const Twine &message) {
              getOperation().emitError() << message.str();
            }))) {
      signalPassFailure();
      return;
    }

    ModuleOp m = getOperation();

    LLVMTypeConverter typeConverter(&context, options);

    // TODO: move addrspace conversion to separate pass
    typeConverter.addTypeAttributeConversion(
        [](mlir::BaseMemRefType type,
           mlir::gpu::AddressSpaceAttr /*memorySpaceAttr*/)
            -> mlir::IntegerAttr {
          auto ctx = type.getContext();
          return mlir::IntegerAttr::get(mlir::IntegerType::get(ctx, 64), 0);
        });

    populateToLLVMAdditionalTypeConversion(typeConverter);

    RewritePatternSet patterns(&context);
    populateFuncToLLVMFuncOpConversionPattern(typeConverter, patterns);
    populateFuncToLLVMConversionPatterns(typeConverter, patterns);
    populateFinalizeMemRefToLLVMConversionPatterns(typeConverter, patterns);
    cf::populateControlFlowToLLVMConversionPatterns(typeConverter, patterns);
    arith::populateArithToLLVMConversionPatterns(typeConverter, patterns);
    populateComplexToLLVMConversionPatterns(typeConverter, patterns);
    ub::populateUBToLLVMConversionPatterns(typeConverter, patterns);

    bool force32BitVectorIndices = false;
    bool reassociateFPReductions = false;
    vector::populateVectorMaskMaterializationPatterns(patterns,
                                                      force32BitVectorIndices);
    populateVectorToLLVMConversionPatterns(typeConverter, patterns,
                                           reassociateFPReductions,
                                           force32BitVectorIndices);

    patterns.insert<AllocOpLowering, DeallocOpLowering, LowerRetainOp,
                    LowerWrapAllocPointerOp, LowerGetAllocToken,
                    AtomicRMWOpLowering, LowerPoison>(typeConverter);

    LLVMConversionTarget target(context);
    target.addIllegalDialect<mlir::func::FuncDialect>();
    target.addIllegalOp<numba::util::RetainOp, mlir::memref::AtomicRMWOp>();

    if (failed(applyPartialConversion(m, target, std::move(patterns))))
      signalPassFailure();

    m->setAttr(LLVM::LLVMDialect::getDataLayoutAttrName(),
               StringAttr::get(m.getContext(),
                               options.dataLayout.getStringRepresentation()));
  }
};

static void populatePreLowerToLlvmPipeline(mlir::OpPassManager &pm) {
  pm.addNestedPass<mlir::func::FuncOp>(std::make_unique<PreLLVMLowering>());
}

static void populateLowerToLlvmPipeline(mlir::OpPassManager &pm) {
  pm.addPass(std::make_unique<RemoveParallelRegionPass>());
  pm.addPass(std::make_unique<LowerParallelToCFGPass>());
  pm.addPass(mlir::createConvertSCFToCFPass());
  pm.addPass(mlir::createCanonicalizerPass());
  pm.addPass(mlir::createConvertComplexToStandardPass());
  pm.addNestedPass<mlir::func::FuncOp>(
      mlir::memref::createExpandStridedMetadataPass());
  pm.addNestedPass<mlir::func::FuncOp>(mlir::createLowerAffinePass());
  pm.addNestedPass<mlir::func::FuncOp>(mlir::arith::createArithExpandOpsPass());
  pm.addNestedPass<mlir::func::FuncOp>(mlir::createConvertMathToLLVMPass());
  pm.addNestedPass<mlir::func::FuncOp>(std::make_unique<LowerVectorOps>());
  pm.addPass(mlir::createConvertMathToLibmPass());
  pm.addPass(numba::createMathExtToLibmPass());
  pm.addPass(numba::createUtilToLLVMPass(&getLLVMOptions));
  pm.addPass(std::make_unique<LLVMLoweringPass>());
  pm.addPass(std::make_unique<FixLLVMStructABIPass>());
  pm.addNestedPass<mlir::LLVM::LLVMFuncOp>(
      std::make_unique<PostLLVMLowering>());
  pm.addNestedPass<mlir::LLVM::LLVMFuncOp>(mlir::createCSEPass());
  pm.addPass(mlir::createCanonicalizerPass());
}
} // namespace

void registerLowerToLLVMPipeline(numba::PipelineRegistry &registry) {
  registry.registerPipeline([](auto sink) {
    auto stage = getLowerLoweringStage();
    sink(preLowerToLLVMPipelineName(), {stage.begin},
         {stage.end, lowerToLLVMPipelineName()}, {},
         &populatePreLowerToLlvmPipeline);
  });
  registry.registerPipeline([](auto sink) {
    auto stage = getLowerLoweringStage();
    sink(lowerToLLVMPipelineName(), {stage.begin, preLowerToLLVMPipelineName()},
         {stage.end}, {}, &populateLowerToLlvmPipeline);
  });
}

llvm::StringRef preLowerToLLVMPipelineName() { return "pre_lower_to_llvm"; }

llvm::StringRef lowerToLLVMPipelineName() { return "lower_to_llvm"; }
