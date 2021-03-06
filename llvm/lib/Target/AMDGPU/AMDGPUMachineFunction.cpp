//===-- AMDGPUMachineFunctionInfo.cpp ---------------------------------------=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AMDGPUMachineFunction.h"
#include "AMDGPUSubtarget.h"
#include "AMDGPUPerfHintAnalysis.h"
#include "llvm/CodeGen/MachineModuleInfo.h"

using namespace llvm;

AMDGPUMachineFunction::AMDGPUMachineFunction(const MachineFunction &MF) :
  MachineFunctionInfo(),
  Mode(MF.getFunction()),
  IsEntryFunction(AMDGPU::isEntryFunctionCC(MF.getFunction().getCallingConv())),
  NoSignedZerosFPMath(MF.getTarget().Options.NoSignedZerosFPMath) {
  const AMDGPUSubtarget &ST = AMDGPUSubtarget::get(MF);

  // FIXME: Should initialize KernArgSize based on ExplicitKernelArgOffset,
  // except reserved size is not correctly aligned.
  const Function &F = MF.getFunction();

  Attribute MemBoundAttr = F.getFnAttribute("amdgpu-memory-bound");
  MemoryBound = MemBoundAttr.isStringAttribute() &&
                MemBoundAttr.getValueAsString() == "true";

  Attribute WaveLimitAttr = F.getFnAttribute("amdgpu-wave-limiter");
  WaveLimiter = WaveLimitAttr.isStringAttribute() &&
                WaveLimitAttr.getValueAsString() == "true";

  CallingConv::ID CC = F.getCallingConv();
  if (CC == CallingConv::AMDGPU_KERNEL || CC == CallingConv::SPIR_KERNEL)
    ExplicitKernArgSize = ST.getExplicitKernArgSize(F, MaxKernArgAlign);
}

unsigned AMDGPUMachineFunction::allocateLDSGlobal(const DataLayout &DL,
                                                  const GlobalVariable &GV) {
  auto Entry = LocalMemoryObjects.insert(std::make_pair(&GV, 0));
  if (!Entry.second)
    return Entry.first->second;

  unsigned Align = GV.getAlignment();
  if (Align == 0)
    Align = DL.getABITypeAlignment(GV.getValueType());

  /// TODO: We should sort these to minimize wasted space due to alignment
  /// padding. Currently the padding is decided by the first encountered use
  /// during lowering.
  unsigned Offset = LDSSize = alignTo(LDSSize, Align);

  Entry.first->second = Offset;
  LDSSize += DL.getTypeAllocSize(GV.getValueType());

  return Offset;
}
