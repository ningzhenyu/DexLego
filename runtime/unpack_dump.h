/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_RUNTIME_UNPACK_DUMP_H_
#define ART_RUNTIME_UNPACK_DUMP_H_

#include <map>

#include "base/mutex.h"
#include "dex_file.h"
#include "invoke_type.h"
#include "stack.h"
#include "dex_instruction-inl.h"
#include "entrypoints/entrypoint_utils-inl.h"
#include "unpack_recording_thread.h"

namespace art {

// #define TIME_EVALUATION

enum DumpItemType {
    D_STRING, D_TYPE, D_PROTO, D_FIELD, D_METHOD, D_CLASS, D_STATIC_VALUE, D_ENCODED_FIELD, D_ENCODED_METHOD, D_CODE
};

struct DumpBase {
  DumpItemType dump_type_;
  uint32_t array_idx_;

  virtual void Output(FILE* file) = 0;
  virtual std::string ToString() = 0;
  virtual size_t HashValue();
  virtual ~DumpBase() {}

  static inline size_t HashInt(uint32_t value);
  static inline size_t CombineHash(size_t h, uint16_t value);
  static inline size_t CombineHash(size_t h, uint32_t value);
  static inline size_t CombineHash(size_t h, std::string s);
};

struct DumpString : DumpBase {
  uint32_t string_length_;
  std::string string_;

  DumpString() {
    dump_type_ = D_STRING;
  }

  virtual void Output(FILE* file);
  virtual std::string ToString();
  bool operator==(const DumpString& rhs) const;
  virtual size_t HashValue();
};

struct DumpType : DumpBase {
  uint32_t descriptor_idx_;  // index into string_ids

  DumpType() {
    dump_type_ = D_TYPE;
  }

  virtual void Output(FILE* file);
  virtual std::string ToString();
  bool operator==(const DumpType& rhs) const;
  virtual size_t HashValue();
};

struct DumpProto : DumpBase {
  uint32_t shorty_idx_;  // index into string_ids array for shorty descriptor
  uint16_t return_type_idx_;  // index into type_ids array for return type
  std::vector<uint16_t> param_types_;

  DumpProto() {
    dump_type_ = D_PROTO;
  }

  virtual void Output(FILE* file);
  virtual std::string ToString();
  bool operator==(const DumpProto& rhs) const;
  virtual size_t HashValue();
};

struct DumpField : DumpBase {
  uint16_t class_idx_;  // index into type_ids_ array for defining class
  uint16_t type_idx_;  // index into type_ids_ array for field type
  uint32_t name_idx_;  // index into string_ids_ array for field name

  DumpField() {
    dump_type_ = D_FIELD;
  }

  virtual void Output(FILE* file);
  virtual std::string ToString();
  bool operator==(const DumpField& rhs) const;
  virtual size_t HashValue();
};

struct DumpMethod : DumpBase {
  uint16_t class_idx_;  // index into type_ids_ array for defining class
  uint16_t proto_idx_;  // index into proto_ids_ array for method prototype
  uint32_t name_idx_;  // index into string_ids_ array for method name

  DumpMethod() {
    dump_type_ = D_METHOD;
  }

  virtual void Output(FILE* file);
  virtual std::string ToString();
  bool operator==(const DumpMethod& rhs) const;
  virtual size_t HashValue();
};

struct DumpClassDef : DumpBase {
  uint16_t class_idx_;  // index into type_ids_ array for this class
  uint32_t access_flags_;
  uint16_t superclass_idx_;  // index into type_ids_ array for superclass
  std::vector<uint16_t> interface_types_;  // file offset to TypeList
  uint32_t source_file_idx_;  // index into string_ids_ for source file name

  // TODO fix this
  // uint32_t annotations_off_;  // file offset to annotations_directory_item
  // std::vector<DumpAnotationItem*> class_annotation_;
  DumpClassDef() {
    dump_type_ = D_CLASS;
  }

  virtual void Output(FILE* file);
  virtual std::string ToString();
  bool operator==(const DumpClassDef& rhs) const;
  virtual size_t HashValue();
};

struct DumpStaticValue : DumpBase {
  uint32_t class_idx_;
  std::map<uint32_t, std::pair<uint16_t, uint8_t*>> values_;

  DumpStaticValue() {
    dump_type_ = D_STATIC_VALUE;
  }

  virtual void Output(FILE* file);
  virtual std::string ToString();
  bool operator==(const DumpStaticValue& rhs) const;
  virtual size_t HashValue();
  virtual ~DumpStaticValue();
};

enum EncodedFieldType {
  STATIC, INSTANCE
};

struct DumpEncodedField : DumpBase {
  EncodedFieldType type_;
  uint32_t field_idx_;  // index into type_ids_ array for defining class
  uint32_t access_flags_;  // index into type_ids_ array for field type

  DumpEncodedField() {
    dump_type_ = D_ENCODED_FIELD;
  }

  virtual void Output(FILE* file);
  virtual std::string ToString();
  bool operator==(const DumpEncodedField& rhs) const;
  virtual size_t HashValue();
};

enum EncodedMethodType {
  DIRECT, VIRTUAL
};

struct DumpEncodedMethod : DumpBase {
  EncodedMethodType type_;
  uint32_t method_idx_;  // index into type_ids_ array for defining class
  uint32_t access_flags_;  // index into type_ids_ array for field type

  DumpEncodedMethod() {
    dump_type_ = D_ENCODED_METHOD;
  }

  virtual void Output(FILE* file);
  virtual std::string ToString();
  bool operator==(const DumpEncodedMethod& rhs) const;
  virtual size_t HashValue();
};

struct DumpCodeItem : DumpBase {
  uint32_t method_idx_;
  uint32_t current_clz_name_idx_;
  uint16_t registers_size_;
  uint16_t ins_size_;
  uint16_t outs_size_;

  // Catch will be handled as normal instruction
  // uint16_t tries_size_;

  // TODO FIX THIS
  // uint32_t debug_info_off_;  // file offset to debug info stream

  uint32_t insns_size_in_code_units_;  // size of the insns array, in 2 byte code units
  uint16_t* insns_;

  DumpCodeItem() {
    dump_type_ = D_CODE;
  }

  virtual void Output(FILE* file);
  virtual std::string ToString();
  bool operator==(const DumpCodeItem& rhs) const;
  virtual size_t HashValue();
  virtual ~DumpCodeItem();
};

template <typename T>
class CompareHelper {
  public:
    explicit CompareHelper(const T* obj) : instance(obj) {}
    bool operator() (const T* rhs) const { return *instance == *rhs; }
  private:
    const T* instance;
};

struct DumpItem {
  std::string path_;
  DumpBase* item_;
};

// enum ForceBranchRet {
//   FORCE_NONE = 0, FORCE_IF = 1, FORCE_ELSE = 2
// };

struct ForceBranch {
  std::string class_;
  std::string name_;
  std::string shorty_;
  std::string return_type_;
  std::vector<std::string> param_types_;
  uint32_t dex_pc_;

//  ForceBranchRet force_target_ = FORCE_NONE;
  int32_t force_offset_ = 0;

  bool reached_ = false;

  bool operator==(const ForceBranch& rhs) const;
  std::string ToString();
};

#ifdef TIME_EVALUATION

enum TimeMeasureType {
  WRITE_F, ADD_Q, ARRAY_C, ARRAY_O, PROCESS_C, HANDLE_L
};

struct TimeMeasure {
  uint64_t write_file_;
  uint64_t add_queue_;
  uint64_t array_find_code_;
  uint64_t array_find_other_;
  uint64_t process_code_;
  uint64_t handle_lock_;

  uint64_t write_file_times_;
  uint64_t add_queue_times_;
  uint64_t array_find_code_times_;
  uint64_t array_find_other_times_;
  uint64_t process_code_times_;
  uint64_t handle_lock_times_;

  TimeMeasure(): write_file_(0), add_queue_(0),
      array_find_code_(0), array_find_other_(0),
      process_code_(0), handle_lock_(0),
      write_file_times_(0), add_queue_times_(0),
      array_find_code_times_(0), array_find_other_times_(0),
      process_code_times_(0), handle_lock_times_(0) {}
};
#endif

class Dumper {
  public:
    static Dumper* Instance();
    ~Dumper();

    bool ForceExecution();

    bool shouldDump();
    bool shouldFilterClass(const char* descriptor);
    void DumpDexFile(std::string location, const uint8_t* base, size_t size);
    void DumpJniLibrary(const std::string location) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
    ArtMethod* GetTargetMethod(mirror::Object* obj) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

    uint32_t GeneralDump(std::string location, const char* file,
        std::map<size_t, uint32_t>& map, DumpBase* data, pthread_mutex_t& lock);

    int32_t GetForceBranch(ArtMethod* method, uint32_t dex_pc) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
    uint32_t StringDump(std::string location, DumpString* s);
    uint16_t TypeDump(std::string location, DumpType* type);
    uint16_t ProtoDump(std::string location, DumpProto* proto);
    uint32_t FieldDump(std::string location, DumpField* field);
    uint32_t MethodDump(std::string location, DumpMethod* method);
    uint16_t ClassDump(std::string location, DumpClassDef* clz);
    uint32_t StaticValueDump(std::string location, DumpStaticValue* sv);
    uint32_t EncodedFieldDump(std::string location, DumpEncodedField* ef);
    uint32_t EncodedMethodDump(std::string location, DumpEncodedMethod* em);
    uint32_t CodeDump(std::string location, DumpCodeItem* code);

    uint32_t DumpStringFromDex(const DexFile& file, uint32_t string_idx) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
    uint16_t DumpTypeFromDex(const DexFile& file, uint16_t type_idx) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
    uint32_t DumpFieldFromDex(const DexFile& dex_file, uint32_t field_idx) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
    uint32_t DumpMethodFromDex(const DexFile& dex_file, uint32_t method_idx) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
    uint32_t DumpClassFromDex(const DexFile& dex_file,
            const DexFile::ClassDef& dex_class_def) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
    uint32_t DumpStaticValuesFromDex(const DexFile& file,
            const DexFile::ClassDef& class_def) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
    uint32_t DumpEncodedFieldFromDex(const DexFile& dex_file, uint32_t field_idx,
            EncodedFieldType type, uint32_t access_flag) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
    uint32_t DumpEncodedMethodFromDex(const DexFile& dex_file, uint32_t method_idx,
            EncodedMethodType type, uint32_t access_flag) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
    std::pair<uint32_t, uint32_t> DumpImplicitEncodedMethod(ArtMethod* method, ShadowFrame& shadow_frame, uint16_t num_reg) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

#ifdef TIME_EVALUATION
    void addTimeMeasure(struct timeval s, struct timeval e, TimeMeasureType type);
#endif

    void InitializeForceBranch();
    void InitializeClassFilter();

  private:
    Dumper();

    bool IsTargetProcess();

    void ToDumpQueueUnblock(DumpItem* item);
//    static void* ToDumpQueue(void* item);
    static void* DumpRun(void* unused);

    std::string path_prefix_;
    pid_t pid_;
    std::string package_name_;
    std::vector<std::string> class_filter_;
    std::vector<std::string> included_class_;

//    std::vector<DumpString*> strings_;
//    std::vector<DumpType*> types_;
//    std::vector<DumpProto*> protos_;
//    std::vector<DumpField*> fields_;
//    std::vector<DumpMethod*> methods_;
//    std::vector<DumpClassDef*> classes_;
//    std::vector<DumpStaticValue*> static_values_;
//    std::vector<DumpEncodedField*> encoded_fields_;
//    std::vector<DumpEncodedMethod*> encoded_methods_;
//    std::vector<DumpCodeItem*> codes_;
    std::map<size_t, uint32_t> strings_;
    std::map<size_t, uint32_t> types_;
    std::map<size_t, uint32_t> protos_;
    std::map<size_t, uint32_t> fields_;
    std::map<size_t, uint32_t> methods_;
    std::map<size_t, uint32_t> classes_;
    std::map<size_t, uint32_t> static_values_;
    std::map<size_t, uint32_t> encoded_fields_;
    std::map<size_t, uint32_t> encoded_methods_;
    std::map<size_t, uint32_t> codes_;
    pthread_mutex_t string_mutex_;
    pthread_mutex_t type_mutex_;
    pthread_mutex_t proto_mutex_;
    pthread_mutex_t field_mutex_;
    pthread_mutex_t method_mutex_;
    pthread_mutex_t class_mutex_;
    pthread_mutex_t static_value_mutex_;
    pthread_mutex_t encoded_field_mutex_;
    pthread_mutex_t encoded_method_mutex_;
    pthread_mutex_t code_mutex_;
//    pthread_mutex_t map_mutex_;
    pthread_t recording_thread_;
    RecordingQueue<DumpItem*> queue_;

    std::vector<ForceBranch*> force_branches_;
    bool force_execution_;
    std::string random_prefix_;

#ifdef TIME_EVALUATION
    TimeMeasure measure_;
    pthread_mutex_t time_mutex_;
#endif

    static Dumper* sInstance;
};

}  // namespace art

#endif  // ART_RUNTIME_UNPACK_DUMP_H_
