/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "interpreter_common.h"
#include "safe_math.h"
#include "unpack_dump_handle.h"

namespace art {
namespace interpreter {

#define HANDLE_PENDING_EXCEPTION()                                                              \
  do {                                                                                          \
    DCHECK(self->IsExceptionPending());                                                         \
    self->AllowThreadSuspension();                                                              \
    uint32_t found_dex_pc = FindNextInstructionFollowingException(self, shadow_frame,           \
                                                                  inst->GetDexPc(insns),        \
                                                                  instrumentation);             \
    if (found_dex_pc == DexFile::kDexNoIndex) {                                                 \
      if (IGNORE_EXCEPTION) {                                                                   \
        self->ClearException();                                                                 \
        inst = inst->RelativeAt(force_execution_offset);                                        \
      } else {                                                                                  \
        HANDLE_EXCEPTION_RETURN();                                                              \
        return JValue(); /* Handled in caller. */                                               \
      }                                                                                         \
    } else {                                                                                    \
      int32_t displacement = static_cast<int32_t>(found_dex_pc) - static_cast<int32_t>(dex_pc); \
      inst = inst->RelativeAt(displacement);                                                    \
    }                                                                                           \
  } while (false)

#define POSSIBLY_HANDLE_PENDING_EXCEPTION(_is_exception_pending, _next_function)  \
  do {                                                                            \
    if (UNLIKELY(_is_exception_pending) && !IGNORE_EXCEPTION) {                   \
      HANDLE_PENDING_EXCEPTION();                                                 \
    } else {                                                                      \
      inst = inst->_next_function();                                              \
    }                                                                             \
  } while (false)

// Code to run before each dex instruction.
#define PREAMBLE()                                                                              \
  do {                                                                                          \
    if (UNLIKELY(instrumentation->HasDexPcListeners())) {                                       \
      instrumentation->DexPcMovedEvent(self, shadow_frame.GetThisObject(code_item->ins_size_),  \
                                       shadow_frame.GetMethod(), dex_pc);                       \
    }                                                                                           \
  } while (false)

template<bool do_access_check, bool transaction_active>
JValue ExecuteSwitchImpl(Thread* self, const DexFile::CodeItem* code_item,
                         ShadowFrame& shadow_frame, JValue result_register) {
  bool do_assignability_check = do_access_check;
  if (UNLIKELY(!shadow_frame.HasReferenceArray())) {
    LOG(FATAL) << "Invalid shadow frame for interpreter use";
    return JValue();
  }
  self->VerifyStack();

  uint32_t dex_pc = shadow_frame.GetDexPC();
  const auto* const instrumentation = Runtime::Current()->GetInstrumentation();
  if (LIKELY(dex_pc == 0)) {  // We are entering the method as opposed to deoptimizing.
    if (kIsDebugBuild) {
        self->AssertNoPendingException();
    }
    if (UNLIKELY(instrumentation->HasMethodEntryListeners())) {
      instrumentation->MethodEnterEvent(self, shadow_frame.GetThisObject(code_item->ins_size_),
                                        shadow_frame.GetMethod(), 0);
    }
  }

  ALLOW_TEMP_MEMORY("ExecuteSwitchImpl");

  const uint16_t* const insns = code_item->insns_;
  const Instruction* inst = Instruction::At(insns + dex_pc);
  uint16_t inst_data;
  while (true) {
    dex_pc = inst->GetDexPc(insns);
    shadow_frame.SetDexPC(dex_pc);
    TraceExecution(shadow_frame, inst, dex_pc);
    inst_data = inst->Fetch16(0);
//    if (method_info) {
//      LOG(ERROR) << "Executing " << inst->Opcode(inst_data) << " " << dex_pc;
//    }
    switch (inst->Opcode(inst_data)) {
      case Instruction::NOP:
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        inst = inst->Next_1xx();
        break;
      case Instruction::MOVE:
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        shadow_frame.SetVReg(inst->VRegA_12x(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::MOVE_FROM16:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVReg(inst->VRegA_22x(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_22x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::MOVE_16:
        PREAMBLE();
        HANDLE_INSTRUCTION(3);
        shadow_frame.SetVReg(inst->VRegA_32x(),
                             shadow_frame.GetVReg(inst->VRegB_32x()));
        inst = inst->Next_3xx();
        break;
      case Instruction::MOVE_WIDE:
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        shadow_frame.SetVRegLong(inst->VRegA_12x(inst_data),
                                 shadow_frame.GetVRegLong(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::MOVE_WIDE_FROM16:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVRegLong(inst->VRegA_22x(inst_data),
                                 shadow_frame.GetVRegLong(inst->VRegB_22x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::MOVE_WIDE_16:
        PREAMBLE();
        HANDLE_INSTRUCTION(3);
        shadow_frame.SetVRegLong(inst->VRegA_32x(),
                                 shadow_frame.GetVRegLong(inst->VRegB_32x()));
        inst = inst->Next_3xx();
        break;
      case Instruction::MOVE_OBJECT:
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        shadow_frame.SetVRegReference(inst->VRegA_12x(inst_data),
                                      shadow_frame.GetVRegReference(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::MOVE_OBJECT_FROM16:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVRegReference(inst->VRegA_22x(inst_data),
                                      shadow_frame.GetVRegReference(inst->VRegB_22x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::MOVE_OBJECT_16:
        PREAMBLE();
        HANDLE_INSTRUCTION(3);
        shadow_frame.SetVRegReference(inst->VRegA_32x(),
                                      shadow_frame.GetVRegReference(inst->VRegB_32x()));
        inst = inst->Next_3xx();
        break;
      case Instruction::MOVE_RESULT:
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        shadow_frame.SetVReg(inst->VRegA_11x(inst_data), result_register.GetI());
        inst = inst->Next_1xx();
        break;
      case Instruction::MOVE_RESULT_WIDE:
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        shadow_frame.SetVRegLong(inst->VRegA_11x(inst_data), result_register.GetJ());
        inst = inst->Next_1xx();
        break;
      case Instruction::MOVE_RESULT_OBJECT:
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        shadow_frame.SetVRegReference(inst->VRegA_11x(inst_data), result_register.GetL());
        inst = inst->Next_1xx();
        break;
      case Instruction::MOVE_EXCEPTION: {
        PREAMBLE();
        HANDLE_MOVE_EXCEPTION_INSTRUCTION(inst->VRegA_11x(inst_data));
        Throwable* exception = self->GetException();
        DCHECK(exception != nullptr) << "No pending exception on MOVE_EXCEPTION instruction";
        shadow_frame.SetVRegReference(inst->VRegA_11x(inst_data), exception);
        self->ClearException();
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::RETURN_VOID_NO_BARRIER: {
        PREAMBLE();
        HANDLE_SPECIAL_RETURN_INSTRUCTION(1);
        JValue result;
        self->AllowThreadSuspension();
        if (UNLIKELY(instrumentation->HasMethodExitListeners())) {
          instrumentation->MethodExitEvent(self, shadow_frame.GetThisObject(code_item->ins_size_),
                                           shadow_frame.GetMethod(), inst->GetDexPc(insns),
                                           result);
        }
        return result;
      }
      case Instruction::RETURN_VOID: {
        PREAMBLE();
        QuasiAtomic::ThreadFenceForConstructor();
        HANDLE_RETURN_INSTRUCTION(1);
        JValue result;
        self->AllowThreadSuspension();
        if (UNLIKELY(instrumentation->HasMethodExitListeners())) {
          instrumentation->MethodExitEvent(self, shadow_frame.GetThisObject(code_item->ins_size_),
                                           shadow_frame.GetMethod(), inst->GetDexPc(insns),
                                           result);
        }
        return result;
      }
      case Instruction::RETURN: {
        PREAMBLE();
        HANDLE_RETURN_INSTRUCTION(1);
        JValue result;
        result.SetJ(0);
        result.SetI(shadow_frame.GetVReg(inst->VRegA_11x(inst_data)));
        self->AllowThreadSuspension();
        if (UNLIKELY(instrumentation->HasMethodExitListeners())) {
          instrumentation->MethodExitEvent(self, shadow_frame.GetThisObject(code_item->ins_size_),
                                           shadow_frame.GetMethod(), inst->GetDexPc(insns),
                                           result);
        }
        return result;
      }
      case Instruction::RETURN_WIDE: {
        PREAMBLE();
        HANDLE_RETURN_INSTRUCTION(1);
        JValue result;
        result.SetJ(shadow_frame.GetVRegLong(inst->VRegA_11x(inst_data)));
        self->AllowThreadSuspension();
        if (UNLIKELY(instrumentation->HasMethodExitListeners())) {
          instrumentation->MethodExitEvent(self, shadow_frame.GetThisObject(code_item->ins_size_),
                                           shadow_frame.GetMethod(), inst->GetDexPc(insns),
                                           result);
        }
        return result;
      }
      case Instruction::RETURN_OBJECT: {
        PREAMBLE();
        HANDLE_RETURN_INSTRUCTION(1);
        JValue result;
        self->AllowThreadSuspension();
        const size_t ref_idx = inst->VRegA_11x(inst_data);
        Object* obj_result = shadow_frame.GetVRegReference(ref_idx);
        if (do_assignability_check && obj_result != nullptr) {
          Class* return_type = shadow_frame.GetMethod()->GetReturnType();
          // Re-load since it might have moved.
          obj_result = shadow_frame.GetVRegReference(ref_idx);
          if (return_type == nullptr) {
            // Return the pending exception.
            HANDLE_PENDING_EXCEPTION();
          }
          if (!obj_result->VerifierInstanceOf(return_type)) {
            // This should never happen.
            std::string temp1, temp2;
            self->ThrowNewExceptionF("Ljava/lang/VirtualMachineError;",
                                     "Returning '%s' that is not instance of return type '%s'",
                                     obj_result->GetClass()->GetDescriptor(&temp1),
                                     return_type->GetDescriptor(&temp2));
            HANDLE_PENDING_EXCEPTION();
          }
        }
        result.SetL(obj_result);
        if (UNLIKELY(instrumentation->HasMethodExitListeners())) {
          instrumentation->MethodExitEvent(self, shadow_frame.GetThisObject(code_item->ins_size_),
                                           shadow_frame.GetMethod(), inst->GetDexPc(insns),
                                           result);
        }
        return result;
      }
      case Instruction::CONST_4: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t dst = inst->VRegA_11n(inst_data);
        int4_t val = inst->VRegB_11n(inst_data);
        shadow_frame.SetVReg(dst, val);
        if (val == 0) {
          shadow_frame.SetVRegReference(dst, nullptr);
        }
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::CONST_16: {
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        uint8_t dst = inst->VRegA_21s(inst_data);
        int16_t val = inst->VRegB_21s();
        shadow_frame.SetVReg(dst, val);
        if (val == 0) {
          shadow_frame.SetVRegReference(dst, nullptr);
        }
        inst = inst->Next_2xx();
        break;
      }
      case Instruction::CONST: {
        PREAMBLE();
        HANDLE_INSTRUCTION(3);
        uint8_t dst = inst->VRegA_31i(inst_data);
        int32_t val = inst->VRegB_31i();
        shadow_frame.SetVReg(dst, val);
        if (val == 0) {
          shadow_frame.SetVRegReference(dst, nullptr);
        }
        inst = inst->Next_3xx();
        break;
      }
      case Instruction::CONST_HIGH16: {
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        uint8_t dst = inst->VRegA_21h(inst_data);
        int32_t val = static_cast<int32_t>(inst->VRegB_21h() << 16);
        shadow_frame.SetVReg(dst, val);
        if (val == 0) {
          shadow_frame.SetVRegReference(dst, nullptr);
        }
        inst = inst->Next_2xx();
        break;
      }
      case Instruction::CONST_WIDE_16:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVRegLong(inst->VRegA_21s(inst_data), inst->VRegB_21s());
        inst = inst->Next_2xx();
        break;
      case Instruction::CONST_WIDE_32:
        PREAMBLE();
        HANDLE_INSTRUCTION(3);
        shadow_frame.SetVRegLong(inst->VRegA_31i(inst_data), inst->VRegB_31i());
        inst = inst->Next_3xx();
        break;
      case Instruction::CONST_WIDE:
        PREAMBLE();
        HANDLE_INSTRUCTION(5);
        shadow_frame.SetVRegLong(inst->VRegA_51l(inst_data), inst->VRegB_51l());
        inst = inst->Next_51l();
        break;
      case Instruction::CONST_WIDE_HIGH16:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVRegLong(inst->VRegA_21h(inst_data),
                                 static_cast<uint64_t>(inst->VRegB_21h()) << 48);
        inst = inst->Next_2xx();
        break;
      case Instruction::CONST_STRING: {
        PREAMBLE();
        HANDLE_INSTRUCTION_ABOUT_STRING(inst->VRegB_21c());
        String* s = ResolveString(self, shadow_frame,  inst->VRegB_21c());
        bool null_object = UNLIKELY(s == nullptr);
        if (null_object && !IGNORE_EXCEPTION) {
          HANDLE_PENDING_EXCEPTION();
        } else {
          if (!null_object) {
            shadow_frame.SetVRegReference(inst->VRegA_21c(inst_data), s);
          }
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::CONST_STRING_JUMBO: {
        PREAMBLE();
        HANDLE_INSTRUCTION_ABOUT_STRING(inst->VRegB_31c());
        String* s = ResolveString(self, shadow_frame,  inst->VRegB_31c());
        bool null_object = UNLIKELY(s == nullptr);
        if (null_object && !IGNORE_EXCEPTION) {
          HANDLE_PENDING_EXCEPTION();
        } else {
          if (!null_object) {
            shadow_frame.SetVRegReference(inst->VRegA_31c(inst_data), s);
          }
          inst = inst->Next_3xx();
        }
        break;
      }
      case Instruction::CONST_CLASS: {
        PREAMBLE();
        HANDLE_INSTRUCTION_ABOUT_TYPE(inst->VRegB_21c(), 2);
        Class* c = ResolveVerifyAndClinit(inst->VRegB_21c(), shadow_frame.GetMethod(),
                                          self, false, do_access_check);
        bool null_object = UNLIKELY(c == nullptr);
        if (null_object && !IGNORE_EXCEPTION) {
          HANDLE_PENDING_EXCEPTION();
        } else {
          if (!null_object) {
            shadow_frame.SetVRegReference(inst->VRegA_21c(inst_data), c);
          }
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::MONITOR_ENTER: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        Object* obj = shadow_frame.GetVRegReference(inst->VRegA_11x(inst_data));
        bool null_object = UNLIKELY(obj == nullptr);
        if (null_object && !IGNORE_EXCEPTION) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
        } else {
          if (!null_object) {
            DoMonitorEnter(self, obj);
          }
          POSSIBLY_HANDLE_PENDING_EXCEPTION(self->IsExceptionPending(), Next_1xx);
        }
        break;
      }
      case Instruction::MONITOR_EXIT: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        Object* obj = shadow_frame.GetVRegReference(inst->VRegA_11x(inst_data));
        bool null_object = UNLIKELY(obj == nullptr);
        if (null_object && !IGNORE_EXCEPTION) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
        } else {
          if (!null_object) {
            DoMonitorExit(self, obj);
          }
          POSSIBLY_HANDLE_PENDING_EXCEPTION(self->IsExceptionPending(), Next_1xx);
        }
        break;
      }
      case Instruction::CHECK_CAST: {
        PREAMBLE();
        HANDLE_INSTRUCTION_ABOUT_TYPE(inst->VRegB_21c(), 2);
        Class* c = ResolveVerifyAndClinit(inst->VRegB_21c(), shadow_frame.GetMethod(),
                                          self, false, do_access_check);
        bool null_object = UNLIKELY(c == nullptr);
        if (null_object && !IGNORE_EXCEPTION) {
          HANDLE_PENDING_EXCEPTION();
        } else {
          if (!null_object) {
            Object* obj = shadow_frame.GetVRegReference(inst->VRegA_21c(inst_data));
            bool check2 = UNLIKELY(obj != nullptr && !obj->InstanceOf(c));
            if (check2 && !IGNORE_EXCEPTION) {
              ThrowClassCastException(c, obj->GetClass());
              HANDLE_PENDING_EXCEPTION();
            } else {
              // HANDLE_INSTRUCTION_ABOUT_TYPE_CAST(inst->VRegA_21c(inst_data), 2);
              inst = inst->Next_2xx();
            }
          } else {
            inst = inst->Next_2xx();
          }
        }
        break;
      }
      case Instruction::INSTANCE_OF: {
        PREAMBLE();
        HANDLE_INSTRUCTION_ABOUT_TYPE(inst->VRegC_22c(), 2);
        Class* c = ResolveVerifyAndClinit(inst->VRegC_22c(), shadow_frame.GetMethod(),
                                          self, false, do_access_check);
        bool null_object = UNLIKELY(c == nullptr);
        if (null_object && !IGNORE_EXCEPTION) {
          HANDLE_PENDING_EXCEPTION();
        } else {
          if (!null_object) {
            Object* obj = shadow_frame.GetVRegReference(inst->VRegB_22c(inst_data));
            shadow_frame.SetVReg(inst->VRegA_22c(inst_data),
                                 (obj != nullptr && obj->InstanceOf(c)) ? 1 : 0);
          }
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::ARRAY_LENGTH:  {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        Object* array = shadow_frame.GetVRegReference(inst->VRegB_12x(inst_data));
        bool null_object = UNLIKELY(array == nullptr);
        if (null_object && !IGNORE_EXCEPTION) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
        } else {
          if (!null_object) {
            shadow_frame.SetVReg(inst->VRegA_12x(inst_data), array->AsArray()->GetLength());
          }
          inst = inst->Next_1xx();
        }
        break;
      }
      case Instruction::NEW_INSTANCE: {
        PREAMBLE();
        HANDLE_INSTRUCTION_ABOUT_TYPE(inst->VRegB_21c(), 2);
        Object* obj = nullptr;
        Class* c = ResolveVerifyAndClinit(inst->VRegB_21c(), shadow_frame.GetMethod(),
                                          self, false, do_access_check);
        if (LIKELY(c != nullptr)) {
          if (UNLIKELY(c->IsStringClass())) {
            gc::AllocatorType allocator_type = Runtime::Current()->GetHeap()->GetCurrentAllocator();
            mirror::SetStringCountVisitor visitor(0);
            obj = String::Alloc<true>(self, 0, allocator_type, visitor);
          } else {
            obj = AllocObjectFromCode<do_access_check, true>(
              inst->VRegB_21c(), shadow_frame.GetMethod(), self,
              Runtime::Current()->GetHeap()->GetCurrentAllocator());
          }
        }
        bool null_object = UNLIKELY(obj == nullptr);
        if (null_object && !IGNORE_EXCEPTION) {
          HANDLE_PENDING_EXCEPTION();
        } else {
          if (!null_object) {
            obj->GetClass()->AssertInitializedOrInitializingInThread(self);
            // Don't allow finalizable objects to be allocated during a transaction since these can't
            // be finalized without a started runtime.
            if (transaction_active && obj->GetClass()->IsFinalizable()) {
              AbortTransactionF(self, "Allocating finalizable object in transaction: %s",
                                PrettyTypeOf(obj).c_str());
              if (!IGNORE_EXCEPTION) {
                HANDLE_PENDING_EXCEPTION();
              } else {
                inst = inst->Next_2xx();
              }
              break;
            }
            shadow_frame.SetVRegReference(inst->VRegA_21c(inst_data), obj);
          }
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::NEW_ARRAY: {
        PREAMBLE();
        HANDLE_INSTRUCTION_ABOUT_TYPE(inst->VRegC_22c(), 2);
        int32_t length = shadow_frame.GetVReg(inst->VRegB_22c(inst_data));
        Object* obj = AllocArrayFromCode<do_access_check, true>(
            inst->VRegC_22c(), length, shadow_frame.GetMethod(), self,
            Runtime::Current()->GetHeap()->GetCurrentAllocator());
        bool null_object = UNLIKELY(obj == nullptr);
        if (null_object && !IGNORE_EXCEPTION) {
          HANDLE_PENDING_EXCEPTION();
        } else {
          if (!null_object) {
            shadow_frame.SetVRegReference(inst->VRegA_22c(inst_data), obj);
          }
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::FILLED_NEW_ARRAY: {
        PREAMBLE();
        HANDLE_INSTRUCTION_ABOUT_TYPE(inst->VRegB_35c(), 3);
        bool success =
            DoFilledNewArray<false, do_access_check, transaction_active>(inst, shadow_frame, self,
                                                                         &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::FILLED_NEW_ARRAY_RANGE: {
        PREAMBLE();
        HANDLE_INSTRUCTION_ABOUT_TYPE(inst->VRegB_3rc(), 3);
        bool success =
            DoFilledNewArray<true, do_access_check, transaction_active>(inst, shadow_frame,
                                                                        self, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::FILL_ARRAY_DATA: {
        PREAMBLE();
        const uint16_t* payload_addr = reinterpret_cast<const uint16_t*>(inst) + inst->VRegB_31t();
        const Instruction::ArrayDataPayload* payload =
            reinterpret_cast<const Instruction::ArrayDataPayload*>(payload_addr);
        Object* obj = shadow_frame.GetVRegReference(inst->VRegA_31t(inst_data));
        HANDLE_FILL_ARRAY_DATA_INSTRUCTION(payload);
        bool success = FillArrayData(obj, payload);
        if (!success && !IGNORE_EXCEPTION) {
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        if (success && transaction_active) {
          RecordArrayElementsInTransaction(obj->AsArray(), payload->element_count);
        }
        inst = inst->Next_3xx();
        break;
      }
      case Instruction::THROW: {
        PREAMBLE();
        HANDLE_THROW_INSTRUCTION(inst->VRegA_11x(inst_data));
        Object* exception = shadow_frame.GetVRegReference(inst->VRegA_11x(inst_data));
        if (UNLIKELY(exception == nullptr)) {
          ThrowNullPointerException("throw with null exception");
        } else if (do_assignability_check && !exception->GetClass()->IsThrowableClass()) {
          // This should never happen.
          std::string temp;
          self->ThrowNewExceptionF("Ljava/lang/VirtualMachineError;",
                                   "Throwing '%s' that is not instance of Throwable",
                                   exception->GetClass()->GetDescriptor(&temp));
        } else {
          self->SetException(exception->AsThrowable());
        }
        HANDLE_PENDING_EXCEPTION();
        break;
      }
      case Instruction::GOTO: {
        PREAMBLE();
        int8_t offset = inst->VRegA_10t(inst_data);
        HANDLE_GOTO_INSTRUCTION(offset);
        if (IsBackwardBranch(offset)) {
          self->AllowThreadSuspension();
        }
        inst = inst->RelativeAt(offset);
        break;
      }
      case Instruction::GOTO_16: {
        PREAMBLE();
        int16_t offset = inst->VRegA_20t();
        HANDLE_GOTO_INSTRUCTION(offset);
        if (IsBackwardBranch(offset)) {
          self->AllowThreadSuspension();
        }
        inst = inst->RelativeAt(offset);
        break;
      }
      case Instruction::GOTO_32: {
        PREAMBLE();
        int32_t offset = inst->VRegA_30t();
        HANDLE_GOTO_INSTRUCTION(offset);
        if (IsBackwardBranch(offset)) {
          self->AllowThreadSuspension();
        }
        inst = inst->RelativeAt(offset);
        break;
      }
      case Instruction::PACKED_SWITCH: {
        PREAMBLE();
        int32_t offset = DoPackedSwitch(inst, shadow_frame, inst_data);
        if (IsBackwardBranch(offset)) {
          self->AllowThreadSuspension();
        }
        HANDLE_SWITCH_INSTRUCTION(shadow_frame.GetVReg(inst->VRegA_31t(inst_data)), offset);
        inst = inst->RelativeAt(offset);
        break;
      }
      case Instruction::SPARSE_SWITCH: {
        PREAMBLE();
        int32_t offset = DoSparseSwitch(inst, shadow_frame, inst_data);
        if (IsBackwardBranch(offset)) {
          self->AllowThreadSuspension();
        }
        HANDLE_SWITCH_INSTRUCTION(shadow_frame.GetVReg(inst->VRegA_31t(inst_data)), offset);
        inst = inst->RelativeAt(offset);
        break;
      }

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

      case Instruction::CMPL_FLOAT: {
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        float val1 = shadow_frame.GetVRegFloat(inst->VRegB_23x());
        float val2 = shadow_frame.GetVRegFloat(inst->VRegC_23x());
        int32_t result;
        if (val1 > val2) {
          result = 1;
        } else if (val1 == val2) {
          result = 0;
        } else {
          result = -1;
        }
        shadow_frame.SetVReg(inst->VRegA_23x(inst_data), result);
        inst = inst->Next_2xx();
        break;
      }
      case Instruction::CMPG_FLOAT: {
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        float val1 = shadow_frame.GetVRegFloat(inst->VRegB_23x());
        float val2 = shadow_frame.GetVRegFloat(inst->VRegC_23x());
        int32_t result;
        if (val1 < val2) {
          result = -1;
        } else if (val1 == val2) {
          result = 0;
        } else {
          result = 1;
        }
        shadow_frame.SetVReg(inst->VRegA_23x(inst_data), result);
        inst = inst->Next_2xx();
        break;
      }
      case Instruction::CMPL_DOUBLE: {
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        double val1 = shadow_frame.GetVRegDouble(inst->VRegB_23x());
        double val2 = shadow_frame.GetVRegDouble(inst->VRegC_23x());
        int32_t result;
        if (val1 > val2) {
          result = 1;
        } else if (val1 == val2) {
          result = 0;
        } else {
          result = -1;
        }
        shadow_frame.SetVReg(inst->VRegA_23x(inst_data), result);
        inst = inst->Next_2xx();
        break;
      }

      case Instruction::CMPG_DOUBLE: {
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        double val1 = shadow_frame.GetVRegDouble(inst->VRegB_23x());
        double val2 = shadow_frame.GetVRegDouble(inst->VRegC_23x());
        int32_t result;
        if (val1 < val2) {
          result = -1;
        } else if (val1 == val2) {
          result = 0;
        } else {
          result = 1;
        }
        shadow_frame.SetVReg(inst->VRegA_23x(inst_data), result);
        inst = inst->Next_2xx();
        break;
      }

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

      case Instruction::CMP_LONG: {
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        int64_t val1 = shadow_frame.GetVRegLong(inst->VRegB_23x());
        int64_t val2 = shadow_frame.GetVRegLong(inst->VRegC_23x());
        int32_t result;
        if (val1 > val2) {
          result = 1;
        } else if (val1 == val2) {
          result = 0;
        } else {
          result = -1;
        }
        shadow_frame.SetVReg(inst->VRegA_23x(inst_data), result);
        inst = inst->Next_2xx();
        break;
      }
      case Instruction::IF_EQ: {
        PREAMBLE();
        if (shadow_frame.GetVReg(inst->VRegA_22t(inst_data)) ==
            shadow_frame.GetVReg(inst->VRegB_22t(inst_data))) {
          int16_t offset = inst->VRegC_22t();
          if (IsBackwardBranch(offset)) {
            self->AllowThreadSuspension();
          }
          HANDLE_IF_INSTRUCTION(offset);
          inst = inst->RelativeAt(offset);
        } else {
          HANDLE_IF_INSTRUCTION(2);
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::IF_NE: {
        PREAMBLE();
        if (shadow_frame.GetVReg(inst->VRegA_22t(inst_data)) !=
            shadow_frame.GetVReg(inst->VRegB_22t(inst_data))) {
          int16_t offset = inst->VRegC_22t();
          if (IsBackwardBranch(offset)) {
            self->AllowThreadSuspension();
          }
          HANDLE_IF_INSTRUCTION(offset);
          inst = inst->RelativeAt(offset);
        } else {
          HANDLE_IF_INSTRUCTION(2);
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::IF_LT: {
        PREAMBLE();
        if (shadow_frame.GetVReg(inst->VRegA_22t(inst_data)) <
            shadow_frame.GetVReg(inst->VRegB_22t(inst_data))) {
          int16_t offset = inst->VRegC_22t();
          if (IsBackwardBranch(offset)) {
            self->AllowThreadSuspension();
          }
          HANDLE_IF_INSTRUCTION(offset);
          inst = inst->RelativeAt(offset);
        } else {
          HANDLE_IF_INSTRUCTION(2);
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::IF_GE: {
        PREAMBLE();
        if (shadow_frame.GetVReg(inst->VRegA_22t(inst_data)) >=
            shadow_frame.GetVReg(inst->VRegB_22t(inst_data))) {
          int16_t offset = inst->VRegC_22t();
          if (IsBackwardBranch(offset)) {
            self->AllowThreadSuspension();
          }
          HANDLE_IF_INSTRUCTION(offset);
          inst = inst->RelativeAt(offset);
        } else {
          HANDLE_IF_INSTRUCTION(2);
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::IF_GT: {
        PREAMBLE();
        if (shadow_frame.GetVReg(inst->VRegA_22t(inst_data)) >
        shadow_frame.GetVReg(inst->VRegB_22t(inst_data))) {
          int16_t offset = inst->VRegC_22t();
          if (IsBackwardBranch(offset)) {
            self->AllowThreadSuspension();
          }
          HANDLE_IF_INSTRUCTION(offset);
          inst = inst->RelativeAt(offset);
        } else {
          HANDLE_IF_INSTRUCTION(2);
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::IF_LE: {
        PREAMBLE();
        if (shadow_frame.GetVReg(inst->VRegA_22t(inst_data)) <=
            shadow_frame.GetVReg(inst->VRegB_22t(inst_data))) {
          int16_t offset = inst->VRegC_22t();
          if (IsBackwardBranch(offset)) {
            self->AllowThreadSuspension();
          }
          HANDLE_IF_INSTRUCTION(offset);
          inst = inst->RelativeAt(offset);
        } else {
          HANDLE_IF_INSTRUCTION(2);
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::IF_EQZ: {
        PREAMBLE();
        if (shadow_frame.GetVReg(inst->VRegA_21t(inst_data)) == 0) {
          int16_t offset = inst->VRegB_21t();
          if (IsBackwardBranch(offset)) {
            self->AllowThreadSuspension();
          }
          HANDLE_IF_INSTRUCTION(offset);
          inst = inst->RelativeAt(offset);
        } else {
          HANDLE_IF_INSTRUCTION(2);
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::IF_NEZ: {
        PREAMBLE();
        if (shadow_frame.GetVReg(inst->VRegA_21t(inst_data)) != 0) {
          int16_t offset = inst->VRegB_21t();
          if (IsBackwardBranch(offset)) {
            self->AllowThreadSuspension();
          }
          HANDLE_IF_INSTRUCTION(offset);
          inst = inst->RelativeAt(offset);
        } else {
          HANDLE_IF_INSTRUCTION(2);
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::IF_LTZ: {
        PREAMBLE();
        if (shadow_frame.GetVReg(inst->VRegA_21t(inst_data)) < 0) {
          int16_t offset = inst->VRegB_21t();
          if (IsBackwardBranch(offset)) {
            self->AllowThreadSuspension();
          }
          HANDLE_IF_INSTRUCTION(offset);
          inst = inst->RelativeAt(offset);
        } else {
          HANDLE_IF_INSTRUCTION(2);
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::IF_GEZ: {
        PREAMBLE();
        if (shadow_frame.GetVReg(inst->VRegA_21t(inst_data)) >= 0) {
          int16_t offset = inst->VRegB_21t();
          if (IsBackwardBranch(offset)) {
            self->AllowThreadSuspension();
          }
          HANDLE_IF_INSTRUCTION(offset);
          inst = inst->RelativeAt(offset);
        } else {
          HANDLE_IF_INSTRUCTION(2);
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::IF_GTZ: {
        PREAMBLE();
        if (shadow_frame.GetVReg(inst->VRegA_21t(inst_data)) > 0) {
          int16_t offset = inst->VRegB_21t();
          if (IsBackwardBranch(offset)) {
            self->AllowThreadSuspension();
          }
          HANDLE_IF_INSTRUCTION(offset);
          inst = inst->RelativeAt(offset);
        } else {
          HANDLE_IF_INSTRUCTION(2);
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::IF_LEZ:  {
        PREAMBLE();
        if (shadow_frame.GetVReg(inst->VRegA_21t(inst_data)) <= 0) {
          int16_t offset = inst->VRegB_21t();
          if (IsBackwardBranch(offset)) {
            self->AllowThreadSuspension();
          }
          HANDLE_IF_INSTRUCTION(offset);
          inst = inst->RelativeAt(offset);
        } else {
          HANDLE_IF_INSTRUCTION(2);
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::AGET_BOOLEAN: {
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        Object* a = shadow_frame.GetVRegReference(inst->VRegB_23x());
        bool null_object = UNLIKELY(a == nullptr);
        if (null_object && !IGNORE_EXCEPTION) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        if (!null_object) {
          int32_t index = shadow_frame.GetVReg(inst->VRegC_23x());
          BooleanArray* array = a->AsBooleanArray();
          if (array->CheckIsValidIndex(index)) {
            shadow_frame.SetVReg(inst->VRegA_23x(inst_data), array->GetWithoutChecks(index));
            inst = inst->Next_2xx();
          } else if (!IGNORE_EXCEPTION) {
            HANDLE_PENDING_EXCEPTION();
          } else {
            inst = inst->Next_2xx();
          }
        } else {
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::AGET_BYTE: {
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        Object* a = shadow_frame.GetVRegReference(inst->VRegB_23x());
        bool null_object = UNLIKELY(a == nullptr);
        if (null_object && !IGNORE_EXCEPTION) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        if (!null_object) {
          int32_t index = shadow_frame.GetVReg(inst->VRegC_23x());
          ByteArray* array = a->AsByteArray();
          if (array->CheckIsValidIndex(index)) {
            shadow_frame.SetVReg(inst->VRegA_23x(inst_data), array->GetWithoutChecks(index));
            inst = inst->Next_2xx();
          } else if (!IGNORE_EXCEPTION) {
            HANDLE_PENDING_EXCEPTION();
          } else {
            inst = inst->Next_2xx();
          }
        } else {
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::AGET_CHAR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        Object* a = shadow_frame.GetVRegReference(inst->VRegB_23x());
        bool null_object = UNLIKELY(a == nullptr);
        if (null_object && !IGNORE_EXCEPTION) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        if (!null_object) {
          int32_t index = shadow_frame.GetVReg(inst->VRegC_23x());
          CharArray* array = a->AsCharArray();
          if (array->CheckIsValidIndex(index)) {
            shadow_frame.SetVReg(inst->VRegA_23x(inst_data), array->GetWithoutChecks(index));
            inst = inst->Next_2xx();
          } else if (!IGNORE_EXCEPTION) {
            HANDLE_PENDING_EXCEPTION();
          } else {
            inst = inst->Next_2xx();
          }
        } else {
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::AGET_SHORT: {
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        Object* a = shadow_frame.GetVRegReference(inst->VRegB_23x());
        bool null_object = UNLIKELY(a == nullptr);
        if (null_object && !IGNORE_EXCEPTION) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        if (!null_object) {
          int32_t index = shadow_frame.GetVReg(inst->VRegC_23x());
          ShortArray* array = a->AsShortArray();
          if (array->CheckIsValidIndex(index)) {
            shadow_frame.SetVReg(inst->VRegA_23x(inst_data), array->GetWithoutChecks(index));
            inst = inst->Next_2xx();
          } else if (!IGNORE_EXCEPTION) {
            HANDLE_PENDING_EXCEPTION();
          } else {
            inst = inst->Next_2xx();
          }
        } else {
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::AGET: {
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        Object* a = shadow_frame.GetVRegReference(inst->VRegB_23x());
        bool null_object = UNLIKELY(a == nullptr);
        if (null_object && !IGNORE_EXCEPTION) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        if (!null_object) {
          int32_t index = shadow_frame.GetVReg(inst->VRegC_23x());
          DCHECK(a->IsIntArray() || a->IsFloatArray()) << PrettyTypeOf(a);
          auto* array = down_cast<IntArray*>(a);
          if (array->CheckIsValidIndex(index)) {
            shadow_frame.SetVReg(inst->VRegA_23x(inst_data), array->GetWithoutChecks(index));
            inst = inst->Next_2xx();
          } else if (!IGNORE_EXCEPTION) {
            HANDLE_PENDING_EXCEPTION();
          } else {
            inst = inst->Next_2xx();
          }
        } else {
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::AGET_WIDE:  {
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        Object* a = shadow_frame.GetVRegReference(inst->VRegB_23x());
        bool null_object = UNLIKELY(a == nullptr);
        if (null_object && !IGNORE_EXCEPTION) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        if (!null_object) {
          int32_t index = shadow_frame.GetVReg(inst->VRegC_23x());
          DCHECK(a->IsLongArray() || a->IsDoubleArray()) << PrettyTypeOf(a);
          auto* array = down_cast<LongArray*>(a);
          if (array->CheckIsValidIndex(index)) {
            shadow_frame.SetVRegLong(inst->VRegA_23x(inst_data), array->GetWithoutChecks(index));
            inst = inst->Next_2xx();
          } else if (!IGNORE_EXCEPTION) {
            HANDLE_PENDING_EXCEPTION();
          } else {
            inst = inst->Next_2xx();
          }
        } else {
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::AGET_OBJECT: {
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        Object* a = shadow_frame.GetVRegReference(inst->VRegB_23x());
        bool null_object = UNLIKELY(a == nullptr);
        if (null_object && !IGNORE_EXCEPTION) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        if (!null_object) {
          int32_t index = shadow_frame.GetVReg(inst->VRegC_23x());
          ObjectArray<Object>* array = a->AsObjectArray<Object>();
          if (array->CheckIsValidIndex(index)) {
            shadow_frame.SetVRegReference(inst->VRegA_23x(inst_data), array->GetWithoutChecks(index));
            inst = inst->Next_2xx();
          } else if (!IGNORE_EXCEPTION) {
            HANDLE_PENDING_EXCEPTION();
          } else {
            inst = inst->Next_2xx();
          }
        } else {
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::APUT_BOOLEAN: {
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        Object* a = shadow_frame.GetVRegReference(inst->VRegB_23x());
        bool null_object = UNLIKELY(a == nullptr);
        if (null_object && !IGNORE_EXCEPTION) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        if (!null_object) {
          uint8_t val = shadow_frame.GetVReg(inst->VRegA_23x(inst_data));
          int32_t index = shadow_frame.GetVReg(inst->VRegC_23x());
          BooleanArray* array = a->AsBooleanArray();
          if (array->CheckIsValidIndex(index)) {
            array->SetWithoutChecks<transaction_active>(index, val);
            inst = inst->Next_2xx();
          } else if (!IGNORE_EXCEPTION) {
            HANDLE_PENDING_EXCEPTION();
          } else {
            inst = inst->Next_2xx();
          }
        } else {
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::APUT_BYTE: {
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        Object* a = shadow_frame.GetVRegReference(inst->VRegB_23x());
        bool null_object = UNLIKELY(a == nullptr);
        if (null_object && !IGNORE_EXCEPTION) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        if (!null_object) {
          int8_t val = shadow_frame.GetVReg(inst->VRegA_23x(inst_data));
          int32_t index = shadow_frame.GetVReg(inst->VRegC_23x());
          ByteArray* array = a->AsByteArray();
          if (array->CheckIsValidIndex(index)) {
            array->SetWithoutChecks<transaction_active>(index, val);
            inst = inst->Next_2xx();
          } else if (!IGNORE_EXCEPTION) {
            HANDLE_PENDING_EXCEPTION();
          } else {
            inst = inst->Next_2xx();
          }
        } else {
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::APUT_CHAR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        Object* a = shadow_frame.GetVRegReference(inst->VRegB_23x());
        bool null_object = UNLIKELY(a == nullptr);
        if (null_object && !IGNORE_EXCEPTION) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        if (!null_object) {
          uint16_t val = shadow_frame.GetVReg(inst->VRegA_23x(inst_data));
          int32_t index = shadow_frame.GetVReg(inst->VRegC_23x());
          CharArray* array = a->AsCharArray();
          if (array->CheckIsValidIndex(index)) {
            array->SetWithoutChecks<transaction_active>(index, val);
            inst = inst->Next_2xx();
          } else if (!IGNORE_EXCEPTION) {
            HANDLE_PENDING_EXCEPTION();
          } else {
            inst = inst->Next_2xx();
          }
        } else {
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::APUT_SHORT: {
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        Object* a = shadow_frame.GetVRegReference(inst->VRegB_23x());
        bool null_object = UNLIKELY(a == nullptr);
        if (null_object && !IGNORE_EXCEPTION) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        if (!null_object) {
          int16_t val = shadow_frame.GetVReg(inst->VRegA_23x(inst_data));
          int32_t index = shadow_frame.GetVReg(inst->VRegC_23x());
          ShortArray* array = a->AsShortArray();
          if (array->CheckIsValidIndex(index)) {
            array->SetWithoutChecks<transaction_active>(index, val);
            inst = inst->Next_2xx();
          } else if (!IGNORE_EXCEPTION) {
            HANDLE_PENDING_EXCEPTION();
          } else {
            inst = inst->Next_2xx();
          }
        } else {
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::APUT: {
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        Object* a = shadow_frame.GetVRegReference(inst->VRegB_23x());
        bool null_object = UNLIKELY(a == nullptr);
        if (null_object && !IGNORE_EXCEPTION) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        if (!null_object) {
          int32_t val = shadow_frame.GetVReg(inst->VRegA_23x(inst_data));
          int32_t index = shadow_frame.GetVReg(inst->VRegC_23x());
          DCHECK(a->IsIntArray() || a->IsFloatArray()) << PrettyTypeOf(a);
          auto* array = down_cast<IntArray*>(a);
          if (array->CheckIsValidIndex(index)) {
            array->SetWithoutChecks<transaction_active>(index, val);
            inst = inst->Next_2xx();
          } else if (!IGNORE_EXCEPTION) {
            HANDLE_PENDING_EXCEPTION();
          } else {
            inst = inst->Next_2xx();
          }
        } else {
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::APUT_WIDE: {
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        Object* a = shadow_frame.GetVRegReference(inst->VRegB_23x());
        bool null_object = UNLIKELY(a == nullptr);
        if (null_object && !IGNORE_EXCEPTION) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        if (!null_object) {
          int64_t val = shadow_frame.GetVRegLong(inst->VRegA_23x(inst_data));
          int32_t index = shadow_frame.GetVReg(inst->VRegC_23x());
          DCHECK(a->IsLongArray() || a->IsDoubleArray()) << PrettyTypeOf(a);
          LongArray* array = down_cast<LongArray*>(a);
          if (array->CheckIsValidIndex(index)) {
            array->SetWithoutChecks<transaction_active>(index, val);
            inst = inst->Next_2xx();
          } else if (!IGNORE_EXCEPTION) {
            HANDLE_PENDING_EXCEPTION();
          } else {
            inst = inst->Next_2xx();
          }
        } else {
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::APUT_OBJECT: {
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        Object* a = shadow_frame.GetVRegReference(inst->VRegB_23x());
        bool null_object = UNLIKELY(a == nullptr);
        if (null_object && !IGNORE_EXCEPTION) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        if (!null_object) {
          int32_t index = shadow_frame.GetVReg(inst->VRegC_23x());
          Object* val = shadow_frame.GetVRegReference(inst->VRegA_23x(inst_data));
          ObjectArray<Object>* array = a->AsObjectArray<Object>();
          if (array->CheckIsValidIndex(index) && array->CheckAssignable(val)) {
            array->SetWithoutChecks<transaction_active>(index, val);
            inst = inst->Next_2xx();
          } else if (!IGNORE_EXCEPTION) {
            HANDLE_PENDING_EXCEPTION();
          } else {
            inst = inst->Next_2xx();
          }
        } else {
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::IGET_BOOLEAN: {
        PREAMBLE();
        bool success = DoFieldGet<InstancePrimitiveRead, Primitive::kPrimBoolean, do_access_check>(
            self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IGET_BYTE: {
        PREAMBLE();
        bool success = DoFieldGet<InstancePrimitiveRead, Primitive::kPrimByte, do_access_check>(
            self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IGET_CHAR: {
        PREAMBLE();
        bool success = DoFieldGet<InstancePrimitiveRead, Primitive::kPrimChar, do_access_check>(
            self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IGET_SHORT: {
        PREAMBLE();
        bool success = DoFieldGet<InstancePrimitiveRead, Primitive::kPrimShort, do_access_check>(
            self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IGET: {
        PREAMBLE();
        bool success = DoFieldGet<InstancePrimitiveRead, Primitive::kPrimInt, do_access_check>(
            self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);

        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IGET_WIDE: {
        PREAMBLE();
        bool success = DoFieldGet<InstancePrimitiveRead, Primitive::kPrimLong, do_access_check>(
            self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IGET_OBJECT: {
        PREAMBLE();
        bool success = DoFieldGet<InstanceObjectRead, Primitive::kPrimNot, do_access_check>(
            self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IGET_QUICK: {
        PREAMBLE();
        bool success = DoIGetQuick<Primitive::kPrimInt>(shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IGET_WIDE_QUICK: {
        PREAMBLE();
        bool success = DoIGetQuick<Primitive::kPrimLong>(shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IGET_OBJECT_QUICK: {
        PREAMBLE();
        bool success = DoIGetQuick<Primitive::kPrimNot>(shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IGET_BOOLEAN_QUICK: {
        PREAMBLE();
        bool success = DoIGetQuick<Primitive::kPrimBoolean>(shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IGET_BYTE_QUICK: {
        PREAMBLE();
        bool success = DoIGetQuick<Primitive::kPrimByte>(shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IGET_CHAR_QUICK: {
        PREAMBLE();
        bool success = DoIGetQuick<Primitive::kPrimChar>(shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IGET_SHORT_QUICK: {
        PREAMBLE();
        bool success = DoIGetQuick<Primitive::kPrimShort>(shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SGET_BOOLEAN: {
        PREAMBLE();
        bool success = DoFieldGet<StaticPrimitiveRead, Primitive::kPrimBoolean, do_access_check>(
            self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SGET_BYTE: {
        PREAMBLE();
        bool success = DoFieldGet<StaticPrimitiveRead, Primitive::kPrimByte, do_access_check>(
            self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SGET_CHAR: {
        PREAMBLE();
        bool success = DoFieldGet<StaticPrimitiveRead, Primitive::kPrimChar, do_access_check>(
            self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SGET_SHORT: {
        PREAMBLE();
        bool success = DoFieldGet<StaticPrimitiveRead, Primitive::kPrimShort, do_access_check>(
            self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SGET: {
        PREAMBLE();
        bool success = DoFieldGet<StaticPrimitiveRead, Primitive::kPrimInt, do_access_check>(
            self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SGET_WIDE: {
        PREAMBLE();
        bool success = DoFieldGet<StaticPrimitiveRead, Primitive::kPrimLong, do_access_check>(
            self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SGET_OBJECT: {
        PREAMBLE();
        bool success = DoFieldGet<StaticObjectRead, Primitive::kPrimNot, do_access_check>(
            self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IPUT_BOOLEAN: {
        PREAMBLE();
        bool success = DoFieldPut<InstancePrimitiveWrite, Primitive::kPrimBoolean, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IPUT_BYTE: {
        PREAMBLE();
        bool success = DoFieldPut<InstancePrimitiveWrite, Primitive::kPrimByte, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IPUT_CHAR: {
        PREAMBLE();
        bool success = DoFieldPut<InstancePrimitiveWrite, Primitive::kPrimChar, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IPUT_SHORT: {
        PREAMBLE();
        bool success = DoFieldPut<InstancePrimitiveWrite, Primitive::kPrimShort, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IPUT: {
        PREAMBLE();
        bool success = DoFieldPut<InstancePrimitiveWrite, Primitive::kPrimInt, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IPUT_WIDE: {
        PREAMBLE();
        bool success = DoFieldPut<InstancePrimitiveWrite, Primitive::kPrimLong, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IPUT_OBJECT: {
        PREAMBLE();
        bool success = DoFieldPut<InstanceObjectWrite, Primitive::kPrimNot, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IPUT_QUICK: {
        PREAMBLE();
        bool success = DoIPutQuick<Primitive::kPrimInt, transaction_active>(
            shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IPUT_BOOLEAN_QUICK: {
        PREAMBLE();
        bool success = DoIPutQuick<Primitive::kPrimBoolean, transaction_active>(
            shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IPUT_BYTE_QUICK: {
        PREAMBLE();
        bool success = DoIPutQuick<Primitive::kPrimByte, transaction_active>(
            shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IPUT_CHAR_QUICK: {
        PREAMBLE();
        bool success = DoIPutQuick<Primitive::kPrimChar, transaction_active>(
            shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IPUT_SHORT_QUICK: {
        PREAMBLE();
        bool success = DoIPutQuick<Primitive::kPrimShort, transaction_active>(
            shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IPUT_WIDE_QUICK: {
        PREAMBLE();
        bool success = DoIPutQuick<Primitive::kPrimLong, transaction_active>(
            shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IPUT_OBJECT_QUICK: {
        PREAMBLE();
        bool success = DoIPutQuick<Primitive::kPrimNot, transaction_active>(
            shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SPUT_BOOLEAN: {
        PREAMBLE();
        bool success = DoFieldPut<StaticPrimitiveWrite, Primitive::kPrimBoolean, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SPUT_BYTE: {
        PREAMBLE();
        bool success = DoFieldPut<StaticPrimitiveWrite, Primitive::kPrimByte, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SPUT_CHAR: {
        PREAMBLE();
        bool success = DoFieldPut<StaticPrimitiveWrite, Primitive::kPrimChar, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SPUT_SHORT: {
        PREAMBLE();
        bool success = DoFieldPut<StaticPrimitiveWrite, Primitive::kPrimShort, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SPUT: {
        PREAMBLE();
        bool success = DoFieldPut<StaticPrimitiveWrite, Primitive::kPrimInt, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SPUT_WIDE: {
        PREAMBLE();
        bool success = DoFieldPut<StaticPrimitiveWrite, Primitive::kPrimLong, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SPUT_OBJECT: {
        PREAMBLE();
        bool success = DoFieldPut<StaticObjectWrite, Primitive::kPrimNot, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::INVOKE_VIRTUAL: {
        PREAMBLE();
        bool success = DoInvoke<kVirtual, false, do_access_check>(
            self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::INVOKE_VIRTUAL_RANGE: {
        PREAMBLE();
        bool success = DoInvoke<kVirtual, true, do_access_check>(
            self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::INVOKE_SUPER: {
        PREAMBLE();
        bool success = DoInvoke<kSuper, false, do_access_check>(
            self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::INVOKE_SUPER_RANGE: {
        PREAMBLE();
        bool success = DoInvoke<kSuper, true, do_access_check>(
            self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::INVOKE_DIRECT: {
        PREAMBLE();
        bool success = DoInvoke<kDirect, false, do_access_check>(
            self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::INVOKE_DIRECT_RANGE: {
        PREAMBLE();
        bool success = DoInvoke<kDirect, true, do_access_check>(
            self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::INVOKE_INTERFACE: {
        PREAMBLE();
        bool success = DoInvoke<kInterface, false, do_access_check>(
            self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::INVOKE_INTERFACE_RANGE: {
        PREAMBLE();
        bool success = DoInvoke<kInterface, true, do_access_check>(
            self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::INVOKE_STATIC: {
        PREAMBLE();
        bool success = DoInvoke<kStatic, false, do_access_check>(
            self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::INVOKE_STATIC_RANGE: {
        PREAMBLE();
        bool success = DoInvoke<kStatic, true, do_access_check>(
            self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::INVOKE_VIRTUAL_QUICK: {
        PREAMBLE();
        bool success = DoInvokeVirtualQuick<false>(
            self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::INVOKE_VIRTUAL_RANGE_QUICK: {
        PREAMBLE();
        bool success = DoInvokeVirtualQuick<true>(
            self, shadow_frame, inst, inst_data, map_and_list, map_and_list ? &code_item->insns_[dex_pc] : nullptr, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::NEG_INT:
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        shadow_frame.SetVReg(
            inst->VRegA_12x(inst_data), -shadow_frame.GetVReg(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::NOT_INT:
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        shadow_frame.SetVReg(
            inst->VRegA_12x(inst_data), ~shadow_frame.GetVReg(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::NEG_LONG:
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        shadow_frame.SetVRegLong(
            inst->VRegA_12x(inst_data), -shadow_frame.GetVRegLong(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::NOT_LONG:
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        shadow_frame.SetVRegLong(
            inst->VRegA_12x(inst_data), ~shadow_frame.GetVRegLong(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::NEG_FLOAT:
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        shadow_frame.SetVRegFloat(
            inst->VRegA_12x(inst_data), -shadow_frame.GetVRegFloat(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::NEG_DOUBLE:
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        shadow_frame.SetVRegDouble(
            inst->VRegA_12x(inst_data), -shadow_frame.GetVRegDouble(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::INT_TO_LONG:
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        shadow_frame.SetVRegLong(inst->VRegA_12x(inst_data),
                                 shadow_frame.GetVReg(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::INT_TO_FLOAT:
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        shadow_frame.SetVRegFloat(inst->VRegA_12x(inst_data),
                                  shadow_frame.GetVReg(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::INT_TO_DOUBLE:
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        shadow_frame.SetVRegDouble(inst->VRegA_12x(inst_data),
                                   shadow_frame.GetVReg(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::LONG_TO_INT:
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        shadow_frame.SetVReg(inst->VRegA_12x(inst_data),
                             shadow_frame.GetVRegLong(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::LONG_TO_FLOAT:
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        shadow_frame.SetVRegFloat(inst->VRegA_12x(inst_data),
                                  shadow_frame.GetVRegLong(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::LONG_TO_DOUBLE:
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        shadow_frame.SetVRegDouble(inst->VRegA_12x(inst_data),
                                   shadow_frame.GetVRegLong(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::FLOAT_TO_INT: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        float val = shadow_frame.GetVRegFloat(inst->VRegB_12x(inst_data));
        int32_t result = art_float_to_integral<int32_t, float>(val);
        shadow_frame.SetVReg(inst->VRegA_12x(inst_data), result);
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::FLOAT_TO_LONG: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        float val = shadow_frame.GetVRegFloat(inst->VRegB_12x(inst_data));
        int64_t result = art_float_to_integral<int64_t, float>(val);
        shadow_frame.SetVRegLong(inst->VRegA_12x(inst_data), result);
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::FLOAT_TO_DOUBLE:
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        shadow_frame.SetVRegDouble(inst->VRegA_12x(inst_data),
                                   shadow_frame.GetVRegFloat(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::DOUBLE_TO_INT: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        double val = shadow_frame.GetVRegDouble(inst->VRegB_12x(inst_data));
        int32_t result = art_float_to_integral<int32_t, double>(val);
        shadow_frame.SetVReg(inst->VRegA_12x(inst_data), result);
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::DOUBLE_TO_LONG: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        double val = shadow_frame.GetVRegDouble(inst->VRegB_12x(inst_data));
        int64_t result = art_float_to_integral<int64_t, double>(val);
        shadow_frame.SetVRegLong(inst->VRegA_12x(inst_data), result);
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::DOUBLE_TO_FLOAT:
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        shadow_frame.SetVRegFloat(inst->VRegA_12x(inst_data),
                                  shadow_frame.GetVRegDouble(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::INT_TO_BYTE:
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        shadow_frame.SetVReg(inst->VRegA_12x(inst_data), static_cast<int8_t>(
            shadow_frame.GetVReg(inst->VRegB_12x(inst_data))));
        inst = inst->Next_1xx();
        break;
      case Instruction::INT_TO_CHAR:
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        shadow_frame.SetVReg(inst->VRegA_12x(inst_data), static_cast<uint16_t>(
            shadow_frame.GetVReg(inst->VRegB_12x(inst_data))));
        inst = inst->Next_1xx();
        break;
      case Instruction::INT_TO_SHORT:
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        shadow_frame.SetVReg(inst->VRegA_12x(inst_data), static_cast<int16_t>(
            shadow_frame.GetVReg(inst->VRegB_12x(inst_data))));
        inst = inst->Next_1xx();
        break;
      case Instruction::ADD_INT: {
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVReg(inst->VRegA_23x(inst_data),
                             SafeAdd(shadow_frame.GetVReg(inst->VRegB_23x()),
                                     shadow_frame.GetVReg(inst->VRegC_23x())));
        inst = inst->Next_2xx();
        break;
      }
      case Instruction::SUB_INT:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVReg(inst->VRegA_23x(inst_data),
                             SafeSub(shadow_frame.GetVReg(inst->VRegB_23x()),
                                     shadow_frame.GetVReg(inst->VRegC_23x())));
        inst = inst->Next_2xx();
        break;
      case Instruction::MUL_INT:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVReg(inst->VRegA_23x(inst_data),
                             SafeMul(shadow_frame.GetVReg(inst->VRegB_23x()),
                                     shadow_frame.GetVReg(inst->VRegC_23x())));
        inst = inst->Next_2xx();
        break;
      case Instruction::DIV_INT: {
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        bool success = DoIntDivide(shadow_frame, inst->VRegA_23x(inst_data),
                                   shadow_frame.GetVReg(inst->VRegB_23x()),
                                   shadow_frame.GetVReg(inst->VRegC_23x()));
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::REM_INT: {
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        bool success = DoIntRemainder(shadow_frame, inst->VRegA_23x(inst_data),
                                      shadow_frame.GetVReg(inst->VRegB_23x()),
                                      shadow_frame.GetVReg(inst->VRegC_23x()));
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SHL_INT:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVReg(inst->VRegA_23x(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_23x()) <<
                             (shadow_frame.GetVReg(inst->VRegC_23x()) & 0x1f));
        inst = inst->Next_2xx();
        break;
      case Instruction::SHR_INT:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVReg(inst->VRegA_23x(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_23x()) >>
                             (shadow_frame.GetVReg(inst->VRegC_23x()) & 0x1f));
        inst = inst->Next_2xx();
        break;
      case Instruction::USHR_INT:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVReg(inst->VRegA_23x(inst_data),
                             static_cast<uint32_t>(shadow_frame.GetVReg(inst->VRegB_23x())) >>
                             (shadow_frame.GetVReg(inst->VRegC_23x()) & 0x1f));
        inst = inst->Next_2xx();
        break;
      case Instruction::AND_INT:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVReg(inst->VRegA_23x(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_23x()) &
                             shadow_frame.GetVReg(inst->VRegC_23x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::OR_INT:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVReg(inst->VRegA_23x(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_23x()) |
                             shadow_frame.GetVReg(inst->VRegC_23x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::XOR_INT:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVReg(inst->VRegA_23x(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_23x()) ^
                             shadow_frame.GetVReg(inst->VRegC_23x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::ADD_LONG:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVRegLong(inst->VRegA_23x(inst_data),
                                 SafeAdd(shadow_frame.GetVRegLong(inst->VRegB_23x()),
                                         shadow_frame.GetVRegLong(inst->VRegC_23x())));
        inst = inst->Next_2xx();
        break;
      case Instruction::SUB_LONG:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVRegLong(inst->VRegA_23x(inst_data),
                                 SafeSub(shadow_frame.GetVRegLong(inst->VRegB_23x()),
                                         shadow_frame.GetVRegLong(inst->VRegC_23x())));
        inst = inst->Next_2xx();
        break;
      case Instruction::MUL_LONG:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVRegLong(inst->VRegA_23x(inst_data),
                                 SafeMul(shadow_frame.GetVRegLong(inst->VRegB_23x()),
                                         shadow_frame.GetVRegLong(inst->VRegC_23x())));
        inst = inst->Next_2xx();
        break;
      case Instruction::DIV_LONG:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        DoLongDivide(shadow_frame, inst->VRegA_23x(inst_data),
                     shadow_frame.GetVRegLong(inst->VRegB_23x()),
                     shadow_frame.GetVRegLong(inst->VRegC_23x()));
        POSSIBLY_HANDLE_PENDING_EXCEPTION(self->IsExceptionPending(), Next_2xx);
        break;
      case Instruction::REM_LONG:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        DoLongRemainder(shadow_frame, inst->VRegA_23x(inst_data),
                        shadow_frame.GetVRegLong(inst->VRegB_23x()),
                        shadow_frame.GetVRegLong(inst->VRegC_23x()));
        POSSIBLY_HANDLE_PENDING_EXCEPTION(self->IsExceptionPending(), Next_2xx);
        break;
      case Instruction::AND_LONG:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVRegLong(inst->VRegA_23x(inst_data),
                                 shadow_frame.GetVRegLong(inst->VRegB_23x()) &
                                 shadow_frame.GetVRegLong(inst->VRegC_23x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::OR_LONG:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVRegLong(inst->VRegA_23x(inst_data),
                                 shadow_frame.GetVRegLong(inst->VRegB_23x()) |
                                 shadow_frame.GetVRegLong(inst->VRegC_23x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::XOR_LONG:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVRegLong(inst->VRegA_23x(inst_data),
                                 shadow_frame.GetVRegLong(inst->VRegB_23x()) ^
                                 shadow_frame.GetVRegLong(inst->VRegC_23x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::SHL_LONG:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVRegLong(inst->VRegA_23x(inst_data),
                                 shadow_frame.GetVRegLong(inst->VRegB_23x()) <<
                                 (shadow_frame.GetVReg(inst->VRegC_23x()) & 0x3f));
        inst = inst->Next_2xx();
        break;
      case Instruction::SHR_LONG:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVRegLong(inst->VRegA_23x(inst_data),
                                 shadow_frame.GetVRegLong(inst->VRegB_23x()) >>
                                 (shadow_frame.GetVReg(inst->VRegC_23x()) & 0x3f));
        inst = inst->Next_2xx();
        break;
      case Instruction::USHR_LONG:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVRegLong(inst->VRegA_23x(inst_data),
                                 static_cast<uint64_t>(shadow_frame.GetVRegLong(inst->VRegB_23x())) >>
                                 (shadow_frame.GetVReg(inst->VRegC_23x()) & 0x3f));
        inst = inst->Next_2xx();
        break;
      case Instruction::ADD_FLOAT:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVRegFloat(inst->VRegA_23x(inst_data),
                                  shadow_frame.GetVRegFloat(inst->VRegB_23x()) +
                                  shadow_frame.GetVRegFloat(inst->VRegC_23x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::SUB_FLOAT:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVRegFloat(inst->VRegA_23x(inst_data),
                                  shadow_frame.GetVRegFloat(inst->VRegB_23x()) -
                                  shadow_frame.GetVRegFloat(inst->VRegC_23x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::MUL_FLOAT:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVRegFloat(inst->VRegA_23x(inst_data),
                                  shadow_frame.GetVRegFloat(inst->VRegB_23x()) *
                                  shadow_frame.GetVRegFloat(inst->VRegC_23x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::DIV_FLOAT:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVRegFloat(inst->VRegA_23x(inst_data),
                                  shadow_frame.GetVRegFloat(inst->VRegB_23x()) /
                                  shadow_frame.GetVRegFloat(inst->VRegC_23x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::REM_FLOAT:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVRegFloat(inst->VRegA_23x(inst_data),
                                  fmodf(shadow_frame.GetVRegFloat(inst->VRegB_23x()),
                                        shadow_frame.GetVRegFloat(inst->VRegC_23x())));
        inst = inst->Next_2xx();
        break;
      case Instruction::ADD_DOUBLE:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVRegDouble(inst->VRegA_23x(inst_data),
                                   shadow_frame.GetVRegDouble(inst->VRegB_23x()) +
                                   shadow_frame.GetVRegDouble(inst->VRegC_23x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::SUB_DOUBLE:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVRegDouble(inst->VRegA_23x(inst_data),
                                   shadow_frame.GetVRegDouble(inst->VRegB_23x()) -
                                   shadow_frame.GetVRegDouble(inst->VRegC_23x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::MUL_DOUBLE:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVRegDouble(inst->VRegA_23x(inst_data),
                                   shadow_frame.GetVRegDouble(inst->VRegB_23x()) *
                                   shadow_frame.GetVRegDouble(inst->VRegC_23x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::DIV_DOUBLE:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVRegDouble(inst->VRegA_23x(inst_data),
                                   shadow_frame.GetVRegDouble(inst->VRegB_23x()) /
                                   shadow_frame.GetVRegDouble(inst->VRegC_23x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::REM_DOUBLE:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVRegDouble(inst->VRegA_23x(inst_data),
                                   fmod(shadow_frame.GetVRegDouble(inst->VRegB_23x()),
                                        shadow_frame.GetVRegDouble(inst->VRegC_23x())));
        inst = inst->Next_2xx();
        break;
      case Instruction::ADD_INT_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVReg(vregA, SafeAdd(shadow_frame.GetVReg(vregA),
                                            shadow_frame.GetVReg(inst->VRegB_12x(inst_data))));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::SUB_INT_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVReg(vregA,
                             SafeSub(shadow_frame.GetVReg(vregA),
                                     shadow_frame.GetVReg(inst->VRegB_12x(inst_data))));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::MUL_INT_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVReg(vregA,
                             SafeMul(shadow_frame.GetVReg(vregA),
                                     shadow_frame.GetVReg(inst->VRegB_12x(inst_data))));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::DIV_INT_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        bool success = DoIntDivide(shadow_frame, vregA, shadow_frame.GetVReg(vregA),
                                   shadow_frame.GetVReg(inst->VRegB_12x(inst_data)));
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_1xx);
        break;
      }
      case Instruction::REM_INT_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        bool success = DoIntRemainder(shadow_frame, vregA, shadow_frame.GetVReg(vregA),
                                      shadow_frame.GetVReg(inst->VRegB_12x(inst_data)));
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_1xx);
        break;
      }
      case Instruction::SHL_INT_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVReg(vregA,
                             shadow_frame.GetVReg(vregA) <<
                             (shadow_frame.GetVReg(inst->VRegB_12x(inst_data)) & 0x1f));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::SHR_INT_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVReg(vregA,
                             shadow_frame.GetVReg(vregA) >>
                             (shadow_frame.GetVReg(inst->VRegB_12x(inst_data)) & 0x1f));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::USHR_INT_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVReg(vregA,
                             static_cast<uint32_t>(shadow_frame.GetVReg(vregA)) >>
                             (shadow_frame.GetVReg(inst->VRegB_12x(inst_data)) & 0x1f));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::AND_INT_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVReg(vregA,
                             shadow_frame.GetVReg(vregA) &
                             shadow_frame.GetVReg(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::OR_INT_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVReg(vregA,
                             shadow_frame.GetVReg(vregA) |
                             shadow_frame.GetVReg(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::XOR_INT_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVReg(vregA,
                             shadow_frame.GetVReg(vregA) ^
                             shadow_frame.GetVReg(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::ADD_LONG_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegLong(vregA,
                                 SafeAdd(shadow_frame.GetVRegLong(vregA),
                                         shadow_frame.GetVRegLong(inst->VRegB_12x(inst_data))));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::SUB_LONG_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegLong(vregA,
                                 SafeSub(shadow_frame.GetVRegLong(vregA),
                                         shadow_frame.GetVRegLong(inst->VRegB_12x(inst_data))));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::MUL_LONG_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegLong(vregA,
                                 SafeMul(shadow_frame.GetVRegLong(vregA),
                                         shadow_frame.GetVRegLong(inst->VRegB_12x(inst_data))));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::DIV_LONG_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        DoLongDivide(shadow_frame, vregA, shadow_frame.GetVRegLong(vregA),
                    shadow_frame.GetVRegLong(inst->VRegB_12x(inst_data)));
        POSSIBLY_HANDLE_PENDING_EXCEPTION(self->IsExceptionPending(), Next_1xx);
        break;
      }
      case Instruction::REM_LONG_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        DoLongRemainder(shadow_frame, vregA, shadow_frame.GetVRegLong(vregA),
                        shadow_frame.GetVRegLong(inst->VRegB_12x(inst_data)));
        POSSIBLY_HANDLE_PENDING_EXCEPTION(self->IsExceptionPending(), Next_1xx);
        break;
      }
      case Instruction::AND_LONG_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegLong(vregA,
                                 shadow_frame.GetVRegLong(vregA) &
                                 shadow_frame.GetVRegLong(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::OR_LONG_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegLong(vregA,
                                 shadow_frame.GetVRegLong(vregA) |
                                 shadow_frame.GetVRegLong(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::XOR_LONG_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegLong(vregA,
                                 shadow_frame.GetVRegLong(vregA) ^
                                 shadow_frame.GetVRegLong(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::SHL_LONG_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegLong(vregA,
                                 shadow_frame.GetVRegLong(vregA) <<
                                 (shadow_frame.GetVReg(inst->VRegB_12x(inst_data)) & 0x3f));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::SHR_LONG_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegLong(vregA,
                                 shadow_frame.GetVRegLong(vregA) >>
                                 (shadow_frame.GetVReg(inst->VRegB_12x(inst_data)) & 0x3f));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::USHR_LONG_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegLong(vregA,
                                 static_cast<uint64_t>(shadow_frame.GetVRegLong(vregA)) >>
                                 (shadow_frame.GetVReg(inst->VRegB_12x(inst_data)) & 0x3f));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::ADD_FLOAT_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegFloat(vregA,
                                  shadow_frame.GetVRegFloat(vregA) +
                                  shadow_frame.GetVRegFloat(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::SUB_FLOAT_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegFloat(vregA,
                                  shadow_frame.GetVRegFloat(vregA) -
                                  shadow_frame.GetVRegFloat(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::MUL_FLOAT_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegFloat(vregA,
                                  shadow_frame.GetVRegFloat(vregA) *
                                  shadow_frame.GetVRegFloat(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::DIV_FLOAT_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegFloat(vregA,
                                  shadow_frame.GetVRegFloat(vregA) /
                                  shadow_frame.GetVRegFloat(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::REM_FLOAT_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegFloat(vregA,
                                  fmodf(shadow_frame.GetVRegFloat(vregA),
                                        shadow_frame.GetVRegFloat(inst->VRegB_12x(inst_data))));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::ADD_DOUBLE_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegDouble(vregA,
                                   shadow_frame.GetVRegDouble(vregA) +
                                   shadow_frame.GetVRegDouble(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::SUB_DOUBLE_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegDouble(vregA,
                                   shadow_frame.GetVRegDouble(vregA) -
                                   shadow_frame.GetVRegDouble(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::MUL_DOUBLE_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegDouble(vregA,
                                   shadow_frame.GetVRegDouble(vregA) *
                                   shadow_frame.GetVRegDouble(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::DIV_DOUBLE_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegDouble(vregA,
                                   shadow_frame.GetVRegDouble(vregA) /
                                   shadow_frame.GetVRegDouble(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::REM_DOUBLE_2ADDR: {
        PREAMBLE();
        HANDLE_INSTRUCTION(1);
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegDouble(vregA,
                                   fmod(shadow_frame.GetVRegDouble(vregA),
                                        shadow_frame.GetVRegDouble(inst->VRegB_12x(inst_data))));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::ADD_INT_LIT16:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVReg(inst->VRegA_22s(inst_data),
                             SafeAdd(shadow_frame.GetVReg(inst->VRegB_22s(inst_data)),
                                     inst->VRegC_22s()));
        inst = inst->Next_2xx();
        break;
      case Instruction::RSUB_INT_LIT16:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVReg(inst->VRegA_22s(inst_data),
                             SafeSub(inst->VRegC_22s(),
                                     shadow_frame.GetVReg(inst->VRegB_22s(inst_data))));
        inst = inst->Next_2xx();
        break;
      case Instruction::MUL_INT_LIT16:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVReg(inst->VRegA_22s(inst_data),
                             SafeMul(shadow_frame.GetVReg(inst->VRegB_22s(inst_data)),
                                     inst->VRegC_22s()));
        inst = inst->Next_2xx();
        break;
      case Instruction::DIV_INT_LIT16: {
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        bool success = DoIntDivide(shadow_frame, inst->VRegA_22s(inst_data),
                                   shadow_frame.GetVReg(inst->VRegB_22s(inst_data)),
                                   inst->VRegC_22s());
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::REM_INT_LIT16: {
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        bool success = DoIntRemainder(shadow_frame, inst->VRegA_22s(inst_data),
                                      shadow_frame.GetVReg(inst->VRegB_22s(inst_data)),
                                      inst->VRegC_22s());
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::AND_INT_LIT16:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVReg(inst->VRegA_22s(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_22s(inst_data)) &
                             inst->VRegC_22s());
        inst = inst->Next_2xx();
        break;
      case Instruction::OR_INT_LIT16:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVReg(inst->VRegA_22s(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_22s(inst_data)) |
                             inst->VRegC_22s());
        inst = inst->Next_2xx();
        break;
      case Instruction::XOR_INT_LIT16:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVReg(inst->VRegA_22s(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_22s(inst_data)) ^
                             inst->VRegC_22s());
        inst = inst->Next_2xx();
        break;
      case Instruction::ADD_INT_LIT8:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVReg(inst->VRegA_22b(inst_data),
                             SafeAdd(shadow_frame.GetVReg(inst->VRegB_22b()), inst->VRegC_22b()));
        inst = inst->Next_2xx();
        break;
      case Instruction::RSUB_INT_LIT8:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVReg(inst->VRegA_22b(inst_data),
                             SafeSub(inst->VRegC_22b(), shadow_frame.GetVReg(inst->VRegB_22b())));
        inst = inst->Next_2xx();
        break;
      case Instruction::MUL_INT_LIT8:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVReg(inst->VRegA_22b(inst_data),
                             SafeMul(shadow_frame.GetVReg(inst->VRegB_22b()), inst->VRegC_22b()));
        inst = inst->Next_2xx();
        break;
      case Instruction::DIV_INT_LIT8: {
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        bool success = DoIntDivide(shadow_frame, inst->VRegA_22b(inst_data),
                                   shadow_frame.GetVReg(inst->VRegB_22b()), inst->VRegC_22b());
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::REM_INT_LIT8: {
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        bool success = DoIntRemainder(shadow_frame, inst->VRegA_22b(inst_data),
                                      shadow_frame.GetVReg(inst->VRegB_22b()), inst->VRegC_22b());
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::AND_INT_LIT8:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVReg(inst->VRegA_22b(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_22b()) &
                             inst->VRegC_22b());
        inst = inst->Next_2xx();
        break;
      case Instruction::OR_INT_LIT8:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVReg(inst->VRegA_22b(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_22b()) |
                             inst->VRegC_22b());
        inst = inst->Next_2xx();
        break;
      case Instruction::XOR_INT_LIT8:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVReg(inst->VRegA_22b(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_22b()) ^
                             inst->VRegC_22b());
        inst = inst->Next_2xx();
        break;
      case Instruction::SHL_INT_LIT8:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVReg(inst->VRegA_22b(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_22b()) <<
                             (inst->VRegC_22b() & 0x1f));
        inst = inst->Next_2xx();
        break;
      case Instruction::SHR_INT_LIT8:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVReg(inst->VRegA_22b(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_22b()) >>
                             (inst->VRegC_22b() & 0x1f));
        inst = inst->Next_2xx();
        break;
      case Instruction::USHR_INT_LIT8:
        PREAMBLE();
        HANDLE_INSTRUCTION(2);
        shadow_frame.SetVReg(inst->VRegA_22b(inst_data),
                             static_cast<uint32_t>(shadow_frame.GetVReg(inst->VRegB_22b())) >>
                             (inst->VRegC_22b() & 0x1f));
        inst = inst->Next_2xx();
        break;
      case Instruction::UNUSED_3E ... Instruction::UNUSED_43:
      case Instruction::UNUSED_F3 ... Instruction::UNUSED_FF:
      case Instruction::UNUSED_79:
      case Instruction::UNUSED_7A:
        UnexpectedOpcode(inst, shadow_frame);
    }
  }
}  // NOLINT(readability/fn_size)

// Explicit definitions of ExecuteSwitchImpl.
template SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) HOT_ATTR
JValue ExecuteSwitchImpl<true, false>(Thread* self, const DexFile::CodeItem* code_item,
                                      ShadowFrame& shadow_frame, JValue result_register);
template SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) HOT_ATTR
JValue ExecuteSwitchImpl<false, false>(Thread* self, const DexFile::CodeItem* code_item,
                                       ShadowFrame& shadow_frame, JValue result_register);
template SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
JValue ExecuteSwitchImpl<true, true>(Thread* self, const DexFile::CodeItem* code_item,
                                     ShadowFrame& shadow_frame, JValue result_register);
template SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
JValue ExecuteSwitchImpl<false, true>(Thread* self, const DexFile::CodeItem* code_item,
                                      ShadowFrame& shadow_frame, JValue result_register);

}  // namespace interpreter
}  // namespace art
