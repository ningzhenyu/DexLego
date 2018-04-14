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

#include <fstream>
#include <sys/stat.h>
#include <sys/time.h>
#include <signal.h>
#include <stdlib.h>

#include "unpack_dump.h"

#include "base/logging.h"
#include "dex_file-inl.h"
#include "art_method.h"
#include "art_method-inl.h"
#include "leb128.h"
#include "mirror/method.h"
#include "mirror/abstract_method.h"

namespace art {

#define STRING_FILE "string"
#define TYPE_FILE "type"
#define PROTO_FILE "proto"
#define FIELD_FILE "field"
#define METHOD_FILE  "method"
#define CLASS_FILE "class"
#define STATIC_VALUE_FILE  "sv"
#define ENCODED_FIELD_FILE  "ef"
#define ENCODED_METHOD_FILE  "em"
#define CODE_FILE  "code"

#define WRITE_FILE
// #undef WRITE_FILE

Dumper* Dumper::sInstance = NULL;

size_t inline DumpBase::HashInt(uint32_t x) {
  x = ((x >> 16) ^ x) * 0x45d9f3b;
  x = ((x >> 16) ^ x) * 0x45d9f3b;
  x = (x >> 16) ^ x;
  return x;
}

size_t inline DumpBase::CombineHash(size_t h, uint16_t value) {
  return CombineHash(h, (uint32_t)value);
}

size_t inline DumpBase::CombineHash(size_t h, uint32_t value) {
  return h * 17 + HashInt(value);
}

size_t inline DumpBase::CombineHash(size_t h, std::string s) {
  std::hash<std::string> hash;
  return h * 17 + HashInt(hash(s));
}

size_t DumpBase::HashValue() {
//  size_t v = 17 + array_idx_ * 17;
//  v = v + (uint32_t)(dump_type_) * 17;
  return 17;
}

void DumpString::Output(FILE* file) {
  fwrite(&array_idx_, 4, 1, file);
  fwrite(&string_length_, 4, 1, file);
  fwrite(string_.data(), string_.size() + 1, 1, file);
}

std::string DumpString::ToString() {
  return std::to_string(string_length_) + "_" + string_;
}

bool DumpString::operator==(const DumpString& rhs) const {
  return string_length_ == rhs.string_length_ && string_.compare(rhs.string_) == 0;
}

size_t DumpString::HashValue() {
  size_t v = DumpBase::HashValue();
  v = CombineHash(v, string_length_);
  v = CombineHash(v, string_);
  return v;
}

void DumpType::Output(FILE* file) {
  fwrite(&array_idx_, 4, 1, file);
  fwrite(&descriptor_idx_, 4, 1, file);
}

std::string DumpType::ToString() {
  return std::to_string(descriptor_idx_);
}

bool DumpType::operator==(const DumpType& rhs) const {
  return descriptor_idx_ == rhs.descriptor_idx_;
}

size_t DumpType::HashValue() {
  size_t v = DumpBase::HashValue();
  v = CombineHash(v, descriptor_idx_);
  return v;
}

void DumpProto::Output(FILE* file) {
  fwrite(&array_idx_, 4, 1, file);
  fwrite(&shorty_idx_, 4, 1, file);
  fwrite(&return_type_idx_, 2, 1, file);
  uint32_t sp = param_types_.size();
  fwrite(&sp, 4, 1, file);
  for (uint16_t t : param_types_) {
    fwrite(&t, 2, 1, file);
  }
}

std::string DumpProto::ToString() {
  return std::to_string(shorty_idx_) + "_" + std::to_string((uint32_t)return_type_idx_);
}

bool DumpProto::operator==(const DumpProto& rhs) const {
  if (shorty_idx_ != rhs.shorty_idx_ || return_type_idx_ != rhs.return_type_idx_) {
    return false;
  }
  uint32_t size = param_types_.size();
  if (size != rhs.param_types_.size()) {
    return false;
  }
  for (uint32_t idx = 0; idx < size; ++idx) {
    if (param_types_[idx] != rhs.param_types_[idx]) {
      return false;
    }
  }
  return true;
}

size_t DumpProto::HashValue() {
  size_t v = DumpBase::HashValue();
  v = CombineHash(v, shorty_idx_);
  v = CombineHash(v, return_type_idx_);
  for (uint16_t i : param_types_) {
    v = CombineHash(v, i);
  }
  return v;
}

void DumpField::Output(FILE* file) {
  fwrite(&array_idx_, 4, 1, file);
  fwrite(&class_idx_, 2, 1, file);
  fwrite(&type_idx_, 2, 1, file);
  fwrite(&name_idx_, 4, 1, file);
}

std::string DumpField::ToString() {
  return std::to_string((uint32_t)class_idx_) + "_" + std::to_string((uint32_t)type_idx_) + "_" + std::to_string(name_idx_);
}

bool DumpField::operator==(const DumpField& rhs) const {
  return class_idx_ == rhs.class_idx_ && type_idx_ == rhs.type_idx_ &&
          name_idx_ == rhs.name_idx_;
}

size_t DumpField::HashValue() {
  size_t v = DumpBase::HashValue();
  v = CombineHash(v, class_idx_);
  v = CombineHash(v, type_idx_);
  v = CombineHash(v, name_idx_);
  return v;
}

void DumpMethod::Output(FILE* file) {
  fwrite(&array_idx_, 4, 1, file);
  fwrite(&class_idx_, 2, 1, file);
  fwrite(&proto_idx_, 2, 1, file);
  fwrite(&name_idx_, 4, 1, file);
}

std::string DumpMethod::ToString() {
  return std::to_string((uint32_t)class_idx_) + "_" + std::to_string((uint32_t)proto_idx_) + "_" + std::to_string(name_idx_);
}

bool DumpMethod::operator==(const DumpMethod& rhs) const {
  return class_idx_ == rhs.class_idx_ && proto_idx_ == rhs.proto_idx_ &&
          name_idx_ == rhs.name_idx_;
}

size_t DumpMethod::HashValue() {
  size_t v = DumpBase::HashValue();
  v = CombineHash(v, class_idx_);
  v = CombineHash(v, proto_idx_);
  v = CombineHash(v, name_idx_);
  return v;
}

void DumpClassDef::Output(FILE* file) {
  fwrite(&array_idx_, 4, 1, file);
  fwrite(&class_idx_, 2, 1, file);
  fwrite(&access_flags_, 4, 1, file);
  fwrite(&superclass_idx_, 2, 1, file);
  uint32_t size = interface_types_.size();
  fwrite(&size, 4, 1, file);
  for (uint16_t idx : interface_types_) {
    fwrite(&idx, 2, 1, file);
  }
  fwrite(&source_file_idx_, 4, 1, file);
}

std::string DumpClassDef::ToString() {
  return std::to_string((uint32_t)class_idx_);
}

bool DumpClassDef::operator==(const DumpClassDef& rhs) const {
  if (class_idx_ != rhs.class_idx_ || access_flags_ != rhs.access_flags_ ||
          superclass_idx_ != rhs.superclass_idx_ || source_file_idx_ != rhs.source_file_idx_) {
    return false;
  }
  uint32_t size = interface_types_.size();
  if (size != rhs.interface_types_.size()) {
    return false;
  }
  for (uint32_t i = 0; i < size; ++i) {
    if (interface_types_[i] != rhs.interface_types_[i]) {
      return false;
    }
  }

  return true;
}

size_t DumpClassDef::HashValue() {
  size_t v = DumpBase::HashValue();
  v = CombineHash(v, class_idx_);
  v = CombineHash(v, access_flags_);
  v = CombineHash(v, superclass_idx_);
  v = CombineHash(v, source_file_idx_);
  for (uint16_t idx : interface_types_) {
    v = CombineHash(v, idx);
  }
  return v;
}

void DumpStaticValue::Output(FILE* file) {
  fwrite(&class_idx_, 4, 1, file);
  uint32_t size = values_.size();
  fwrite(&size, 4, 1, file);
  for (auto item : values_) {
    fwrite(&item.first, 4, 1, file);
    fwrite(&item.second.first, 2, 1, file);
    fwrite(item.second.second, item.second.first, 1, file);
  }
}

std::string DumpStaticValue::ToString() {
  return std::to_string(class_idx_);
}

bool DumpStaticValue::operator==(const DumpStaticValue& rhs) const {
  if (class_idx_ != rhs.class_idx_) {
    return false;
  }
  uint32_t size = values_.size();
  if (size != rhs.values_.size()) {
    return false;
  }
  auto it = values_.begin();
  auto old = rhs.values_.begin();
  for (uint32_t i = 0; i < size; ++i) {
    if (it->first != old->first) {
      return false;
    }
    uint32_t s2 = it->second.first;
    if (s2 != old->second.first) {
      return false;
    }
    for (uint32_t j = 0; j < s2; ++j) {
      if (it->second.second[j] != old->second.second[j]) {
        return false;
      }
    }
  }
  return true;
}

size_t DumpStaticValue::HashValue() {
  size_t v = DumpBase::HashValue();
  return CombineHash(v, class_idx_);
}

DumpStaticValue::~DumpStaticValue() {
  for (auto item : values_) {
    delete item.second.second;
  }
  values_.clear();
}

void DumpCodeItem::Output(FILE* file) {
  fwrite(&method_idx_, 4, 1, file);
  fwrite(&current_clz_name_idx_, 4, 1, file);
  fwrite(&registers_size_, 2, 1, file);
  fwrite(&ins_size_, 2, 1, file);
  fwrite(&outs_size_, 2, 1, file);
  fwrite(&insns_size_in_code_units_, 4, 1, file);
  fwrite(insns_, insns_size_in_code_units_ * 2, 1, file);
}

std::string DumpCodeItem::ToString() {
  return std::to_string(method_idx_);
}

bool DumpCodeItem::operator==(const DumpCodeItem& rhs) const {
  if (registers_size_ != rhs.registers_size_ || ins_size_ != rhs.ins_size_ ||
      outs_size_ != rhs.outs_size_ || insns_size_in_code_units_ != rhs.insns_size_in_code_units_ ||
      method_idx_ != rhs.method_idx_) {
    return false;
  }
  // LOG(ERROR) << "DumpCodeItem::operator== " << insns_size_in_code_units_ << " "
  //     << rhs.insns_size_in_code_units_ << " " << insns_ << " " << rhs.insns_;
  for (uint32_t i = 0; i < insns_size_in_code_units_; ++i) {
    // LOG(ERROR) << "DumpCodeItem::operator== " << i;
    if (insns_[i] != rhs.insns_[i]) {
      return false;
    }
  }
  return true;
}

size_t DumpCodeItem::HashValue() {
  size_t v = DumpBase::HashValue();
  v = CombineHash(v, registers_size_);
  v = CombineHash(v, ins_size_);
  v = CombineHash(v, outs_size_);
  v = CombineHash(v, insns_size_in_code_units_);
  v = CombineHash(v, method_idx_);

  for (uint32_t i = 0; i < insns_size_in_code_units_; ++i) {
    v = CombineHash(v, insns_[i]);
  }
  return v;
}

DumpCodeItem::~DumpCodeItem() {
  delete insns_;
}

void DumpEncodedField::Output(FILE* file) {
  fwrite(&type_, 4, 1, file);
  fwrite(&field_idx_, 4, 1, file);
  fwrite(&access_flags_, 4, 1, file);
}

std::string DumpEncodedField::ToString() {
  return std::to_string((uint32_t)type_) + "_" + std::to_string(field_idx_) + "_" + std::to_string(access_flags_);
}

bool DumpEncodedField::operator==(const DumpEncodedField& rhs) const {
  return type_ == rhs.type_ && field_idx_ == rhs.field_idx_;
}

size_t DumpEncodedField::HashValue() {
  size_t v = DumpBase::HashValue();
  v = CombineHash(v, (uint32_t)type_);
  v = CombineHash(v, field_idx_);
  return v;
}

void DumpEncodedMethod::Output(FILE* file) {
  fwrite(&type_, 4, 1, file);
  fwrite(&method_idx_, 4, 1, file);
  fwrite(&access_flags_, 4, 1, file);
}

std::string DumpEncodedMethod::ToString() {
  return std::to_string((uint32_t)type_) + "_" + std::to_string(method_idx_) + "_" + std::to_string(access_flags_);
}

bool DumpEncodedMethod::operator==(const DumpEncodedMethod& rhs) const {
  return type_ == rhs.type_ && method_idx_ == rhs.method_idx_;
}

size_t DumpEncodedMethod::HashValue() {
  size_t v = DumpBase::HashValue();
  v = CombineHash(v, (uint32_t)type_);
  v = CombineHash(v, method_idx_);
  return v;
}

std::string ForceBranch::ToString() {
  return class_ + " " + name_ + " " + shorty_ + " " + std::to_string((uint32_t)dex_pc_) + "," + std::to_string(force_offset_);
}

bool ForceBranch::operator==(const ForceBranch& rhs) const {
  if (dex_pc_ != rhs.dex_pc_) {
//    LOG(ERROR) << "different dex_pc " << std::to_string(dex_pc_) << "," << std::to_string(rhs.dex_pc_);
    return false;
  }
  if (class_.compare(rhs.class_) != 0) {
//    LOG(ERROR) << "different class_ " << class_ << "," << rhs.class_;
    return false;
  }
  if (name_.compare(rhs.name_) != 0) {
//    LOG(ERROR) << "different name_ " << name_ << "," << rhs.name_;
    return false;
  }
  if (shorty_.compare(rhs.shorty_) != 0) {
//    LOG(ERROR) << "different shorty_ " << shorty_ << "," << rhs.shorty_;
    return false;
  }
  if (return_type_.compare(rhs.return_type_) != 0) {
//    LOG(ERROR) << "different return_type_ " << return_type_ << "," << rhs.return_type_;
    return false;
  }


//  if (class_.compare(rhs.class_) != 0 || name_.compare(rhs.name_) != 0 ||
//      shorty_.compare(rhs.shorty_) != 0 || return_type_.compare(rhs.return_type_) != 0) {
//    return false;
//  }
  uint32_t size = param_types_.size();
  if (size != rhs.param_types_.size()) {
//    LOG(ERROR) << "different param size " << std::to_string(size) << "," << std::to_string(rhs.param_types_.size());
    return false;
  }
  for (uint32_t i = 0; i < size; ++i) {
    if (param_types_[i].compare(rhs.param_types_[i]) != 0) {
//      LOG(ERROR) << "different param " << param_types_[i] << " " << rhs.param_types_[i];
//          << " " << std::to_string(param_types_[i].length()) << " " << std::to_string(rhs.param_types_[i].length()) <<
//          " " << std::to_string(param_types_[i].compare(rhs.param_types_[i]));
      return false;
    }
  }

  return true;
}

int32_t Dumper::GetForceBranch(ArtMethod* method, uint32_t dex_pc) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
// ForceBranchRet Dumper::GetForceBranch(ArtMethod* method, uint32_t dex_pc) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  if (!ForceExecution()) {
    return 0;
//    return FORCE_NONE;
  }
  uint32_t size = force_branches_.size();
  if (0 == size) {
    return 0;
//    return FORCE_NONE;
  }

  ForceBranch current;
  current.dex_pc_ = dex_pc;
  const DexFile& file = *method->GetDexFile();
  uint32_t method_idx = method->GetDexMethodIndex();
  uint32_t length = 0;

  const DexFile::MethodId& method_id = file.GetMethodId(method_idx);

  const DexFile::TypeId& class_type_id = file.GetTypeId(method_id.class_idx_);
  const DexFile::StringId& class_string_id = file.GetStringId(class_type_id.descriptor_idx_);
  current.class_ = std::string(file.GetStringDataAndUtf16Length(class_string_id, &length));

  const DexFile::StringId& name_string_id = file.GetStringId(method_id.name_idx_);
  current.name_ = std::string(file.GetStringDataAndUtf16Length(name_string_id, &length));

  const DexFile::ProtoId& proto_id = file.GetProtoId(method_id.proto_idx_);

  const DexFile::StringId& shorty_string_id = file.GetStringId(proto_id.shorty_idx_);
  current.shorty_ = std::string(file.GetStringDataAndUtf16Length(shorty_string_id, &length));

  const DexFile::TypeId& return_type_type_id = file.GetTypeId(proto_id.return_type_idx_);
  const DexFile::StringId& return_type_string_id = file.GetStringId(return_type_type_id.descriptor_idx_);
  current.return_type_ = std::string(file.GetStringDataAndUtf16Length(return_type_string_id, &length));

  if (proto_id.parameters_off_ != 0) {
    const DexFile::TypeList* list = file.GetProtoParameters(proto_id);
    uint32_t param_size = list->Size();
    uint32_t idx;
    for (idx = 0; idx < param_size; ++idx) {
      const DexFile::TypeItem& item = list->GetTypeItem(idx);
      const DexFile::TypeId& param_type_id = file.GetTypeId(item.type_idx_);
      const DexFile::StringId& param_string_id = file.GetStringId(param_type_id.descriptor_idx_);
      std::string param_string = std::string(file.GetStringDataAndUtf16Length(param_string_id, &length));
      current.param_types_.push_back(param_string);
    }
  }

  if (!strstr(current.name_.c_str(), "jacoco")) {
    LOG(ERROR) << "Try to find ifBranch:" << current.ToString() << " " << dex_pc;
  }

  for (uint32_t i = 0; i < size; ++i) {
    ForceBranch* branch = force_branches_[i];
    if (current == *branch) {
      if (branch->reached_) {
        LOG(ERROR) << "ForceBranch found, but reached";
      } else {
        LOG(ERROR) << "ForceBranch found:" << branch->ToString();
        branch->reached_ = true;
        return branch->force_offset_;
      }
    }
  }

  return 0;
//  return FORCE_NONE;
}

void Dumper::ToDumpQueueUnblock(DumpItem* item) {
//  pthread_t t_id;
//  int rc = pthread_create(&t_id, NULL, ToDumpQueue, reinterpret_cast<void*>(item));
//  if (rc) {
//    LOG(FATAL) << "create thread for queue failed! " << rc;
//  } else {
//    rc = pthread_detach(t_id);
//    if (rc) {
//      LOG(FATAL) << "detach thread failed! " << rc;
//    }
//  }
  queue_.add(item);
}

//  void* Dumper::ToDumpQueue(void* item) {
//    sInstance->queue_.add(reinterpret_cast<DumpItem*>(item));
//    return nullptr;
//  }

void* Dumper::DumpRun(__attribute__((unused))void* unused) {
  while (true) {
    DumpItem* item = sInstance->queue_.remove();
#ifdef TIME_EVALUATION
    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
#endif
    FILE* dump_file = fopen(item->path_.c_str(), "ab+");
    item->item_->Output(dump_file);
    fflush(dump_file);
    fclose(dump_file);
#ifdef TIME_EVALUATION
    gettimeofday(&t2, NULL);
    sInstance->addTimeMeasure(t1, t2, WRITE_F);
#endif
    delete item;
  }
}

void sig_handler(int signum) {
  LOG(ERROR) << "Received signal " << std::to_string(signum);
  Dumper::Instance()->InitializeForceBranch();
}

Dumper::Dumper() {
  package_name_ = "";

  if (IsTargetProcess()) {
    char path[100];
    sprintf(path, "/data/data/%s/revealer", package_name_.c_str());
    mkdir(path, 0777);

    InitializeForceBranch();
    InitializeClassFilter();

    int rc = pthread_create(&recording_thread_, NULL, DumpRun, NULL);
    if (rc) {
      LOG(FATAL) << "create thread for recording failed! " << rc;
    }
    pthread_mutex_init(&string_mutex_, NULL);
    pthread_mutex_init(&type_mutex_, NULL);
    pthread_mutex_init(&proto_mutex_, NULL);
    pthread_mutex_init(&field_mutex_, NULL);
    pthread_mutex_init(&method_mutex_, NULL);
    pthread_mutex_init(&class_mutex_, NULL);
    pthread_mutex_init(&static_value_mutex_, NULL);
    pthread_mutex_init(&encoded_field_mutex_, NULL);
    pthread_mutex_init(&encoded_method_mutex_, NULL);
    pthread_mutex_init(&code_mutex_, NULL);
//    pthread_mutex_init(&map_mutex_, NULL);
#ifdef TIME_EVALUATION
    pthread_mutex_init(&time_mutex_, NULL);
#endif

    struct timeval t1;
    gettimeofday(&t1, NULL);
    srand(t1.tv_sec * 1000 + t1.tv_usec / 1000);
    char digits[7];
    sprintf(digits, "%.6d", rand() % 1000000);
    random_prefix_ = std::string(digits);

    signal(44, sig_handler);
  }
}

Dumper::~Dumper() {
  sInstance = NULL;
}

Dumper* Dumper::Instance() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  if (!sInstance) {
    sInstance = new Dumper();
  }
  return sInstance;
}

bool Dumper::IsTargetProcess() {
  char fname[128];
  char fname2[128];
  char buff[1024];
  pid_t pid = getpid();
  sprintf(fname, "/proc/%d/cmdline", pid);
  FILE *fp = fopen(fname, "r");
  if (fp != NULL) {
    // LOG(ERROR) << "get into get pid " << pid;
    while (fgets(buff, 1024, fp)) {
      LOG(ERROR) << "fgets:" << buff;
      std::string pkg_name = std::string(buff);
      size_t pos = pkg_name.find(":");
      if (pos != std::string::npos) {
        pkg_name = pkg_name.substr(0, pos);
      }
      sprintf(fname2, "/data/data/%s/reveal_filter", pkg_name.c_str());
      std::ifstream filter(fname2);
      if (filter.fail()) {
        if (strstr(buff, "zhenyu") || strstr(buff, "ecspride") || strstr(buff, "mit")
            || strstr(buff, "example") || strstr(buff, "cert") || strstr(buff, "snt")
            || strstr(buff, "wayne")) {
          LOG(ERROR) << "default filter in dumper";
          package_name_ = std::string(pkg_name);
          pid_ = pid;
          return true;
        }
      } else {
        LOG(ERROR) << "file filter in dumper";
        std::string line_filter;
        while (std::getline(filter, line_filter)) {
          if (strstr(buff, line_filter.c_str())) {
            package_name_ = std::string(pkg_name);
            pid_ = pid;
            return true;
          }
        }
      }
      if (filter.is_open()) {
        filter.close();
      }
      break;
    }
    fclose(fp);
    return false;
  } else {
    LOG(ERROR) << "file not found for pid " << pid;
    return false;
  }
}

std::vector<std::string> split(const std::string &text, char sep) {
  std::vector<std::string> tokens;
  std::size_t start = 0, end = 0;
  while ((end = text.find(sep, start)) != std::string::npos) {
    if (end != start) {
      tokens.push_back(text.substr(start, end - start));
    }
    start = end + 1;
  }
  if (end != start) {
    tokens.push_back(text.substr(start));
  }
  return tokens;
}

void Dumper::InitializeForceBranch() {
  force_execution_ = false;
  force_branches_.clear();
  char fname[128];
  char buff[1024];
  sprintf(fname, "/data/data/%s/force_branches", package_name_.c_str());
  FILE *fp = fopen(fname, "r");
  if (fp != NULL) {
    // LOG(ERROR) << "get into get pid " << pid;
    while (fgets(buff, 1024, fp)) {
      std::string line = std::string(buff);
      // remove \n
      line = line.substr(0, line.length() - 1);
      if (line.length() > 0) {
        std::vector<std::string> items = split(line, ' ');
        uint32_t item_size = items.size();
        if (item_size > 6) {
          uint32_t param_size = stoi(items[6]);
          if (item_size == param_size + 7) {
            ForceBranch* branch = new ForceBranch;
            branch->class_ = items[0];
            branch->name_ = items[1];
            branch->shorty_ = items[2];
            branch->return_type_ = items[3];
            branch->dex_pc_ = static_cast<uint32_t>(stoi(items[4]));
            branch->force_offset_ = static_cast<int32_t>(stoi(items[5]));
            uint32_t idx;
            for (idx = 0; idx < param_size; ++idx) {
              branch->param_types_.push_back(items[7 + idx]);
            }
            force_branches_.push_back(branch);
            LOG(ERROR) << "Add force Branch:" << branch->ToString();
            force_execution_ = true;
          } else {
            LOG(ERROR) << "wrong param_size and item_size " << param_size << "," << item_size;
          }
        } else {
          LOG(ERROR) << "too less items " << item_size;
        }
      } else {
        LOG(ERROR) << "empty line";
      }
    }
    fclose(fp);
  }
}

void Dumper::InitializeClassFilter() {
  char fname[128];
  sprintf(fname, "/data/data/%s/class_filter", package_name_.c_str());
  std::ifstream filter(fname);
  if (!filter.fail()) {
    LOG(ERROR) << "init class_filter";
    std::string line_filter;
    while (std::getline(filter, line_filter)) {
      class_filter_.push_back(line_filter);
    }
  }
  if (filter.is_open()) {
    filter.close();
  }

  sprintf(fname, "/data/data/%s/included_class", package_name_.c_str());
  std::ifstream filter2(fname);
  if (!filter2.fail()) {
    LOG(ERROR) << "init class_filter";
    std::string line_filter;
    while (std::getline(filter2, line_filter)) {
      included_class_.push_back(line_filter);
    }
  }
  if (filter2.is_open()) {
    filter2.close();
  }
}

bool Dumper::shouldDump() {
  return !package_name_.empty();
}

bool Dumper::ForceExecution() {
  return force_execution_;
}

bool Dumper::shouldFilterClass(const char* descriptor) {
  if (included_class_.size() > 0) {
    bool found = false;
    for (std::string s : included_class_) {
      if (strstr(descriptor, s.c_str())) {
        found = true;
        break;
      }
    }
//    LOG(ERROR) << "shouldFilterClass? " << descriptor << " " << !found;
    return !found;
  } else if (class_filter_.size() > 0) {
    for (std::string s : class_filter_) {
      if (strstr(descriptor, s.c_str())) {
//        LOG(ERROR) << "shouldFilterClass? " << descriptor << " true";
        return true;
      }
    }
  }
//  LOG(ERROR) << "shouldFilterClass? " << descriptor << " false";
  return false;
}

ArtMethod* Dumper::GetTargetMethod(mirror::Object* obj) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  LOG(ERROR) << "HandleReflection " << obj;
  LOG(ERROR) << "class " << obj->GetClass();
  LOG(ERROR) << "name " << obj->GetClass()->GetName()->ToModifiedUtf8();
  auto target = down_cast<mirror::AbstractMethod*>(obj);
  LOG(ERROR) << "get target " << target;
  LOG(ERROR) << "get target " << target->GetClass();
  LOG(ERROR) << "get target " << target->GetClass()->GetName()->ToModifiedUtf8();
  ArtMethod* ret = target->GetArtMethod();

  mirror::Class* declaring_class = ret->GetDeclaringClass();
  if (UNLIKELY(!declaring_class->IsInitialized())) {
    StackHandleScope<1> hs(Thread::Current());
    Handle<mirror::Class> h_class(hs.NewHandle(declaring_class));
    if (!Runtime::Current()->GetClassLinker()->EnsureInitialized(Thread::Current(), h_class, true, true)) {
      return nullptr;
    }
  }
  LOG(ERROR) << "get ret " << ret;
  LOG(ERROR) << "get ret " << ret->GetDexMethodIndex();
  LOG(ERROR) << "get ret " << ret->ShouldManipulate();
  LOG(ERROR) << "Find target method ";
  return ret;
}

void Dumper::DumpDexFile(std::string location, const uint8_t* base, size_t size) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  char* path = new char[100];
  std::hash<std::string> hash;
  sprintf(path, "/data/data/%s/dump_%zu.dex", package_name_.c_str(), hash(location.c_str()));
  LOG(ERROR) << "try to copy " << location << " " << size << " " << path;
  FILE* dump_file = fopen(path, "wb");
  if (dump_file) {
    fwrite(base, size, 1, dump_file);
    fflush(dump_file);
    fclose(dump_file);
  } else {
    LOG(ERROR) << "create or open " << path << " failed";
  }
  LOG(ERROR) << "copy " << location << " finished";
}

void Dumper::DumpJniLibrary(const std::string location)
SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  char* path = new char[100];
  std::hash<std::string> hash;
  sprintf(path, "/data/data/%s/dump_%zu.so", package_name_.c_str(), hash(location.c_str()));
  LOG(ERROR) << "try to copy " << location << " to " << path;
  FILE* dump_file = fopen(path, "wb");
  FILE* source = fopen(location.c_str(), "rb");
  if (dump_file && source) {
    char buf[1024];
    size_t size;

    while ((size = fread(buf, 1, 1024, source)) > 0) {
      fwrite(buf, 1, size, dump_file);
    }

    fflush(dump_file);
    fclose(dump_file);
    fclose(source);
  } else {
    if (dump_file) {
      LOG(ERROR) << "open " << location << " failed";
      fclose(dump_file);
    }
    if (source) {
      LOG(ERROR) << "create " << path << " failed";
      fclose(source);
    }
  }
  LOG(ERROR) << "copy " << location << " finished";
}

uint32_t Dumper::GeneralDump(std::string location, const char* file,
    std::map<size_t, uint32_t>& map, DumpBase* data, pthread_mutex_t& lock) {
  uint32_t ret;
#ifdef TIME_EVALUATION
  struct timeval t1, t2;
  gettimeofday(&t1, NULL);
#endif
  pthread_mutex_lock(&lock);
#ifdef TIME_EVALUATION
  gettimeofday(&t2, NULL);
  addTimeMeasure(t1, t2, HANDLE_L);
  gettimeofday(&t1, NULL);
#endif
  size_t hash_value = data->HashValue();
  auto it = map.find(hash_value);
#ifdef TIME_EVALUATION
  gettimeofday(&t2, NULL);
  if (data->dump_type_ == D_CODE) {
    addTimeMeasure(t1, t2, ARRAY_C);
  } else {
    addTimeMeasure(t1, t2, ARRAY_O);
  }
#endif
  if (it != map.end()) {
    ret = it->second;
    // LOG(ERROR) << "found " << (uint32_t)data->dump_type_ << "," << data->ToString() << "," << hash_value << "," << ret;
    delete data;
#ifdef TIME_EVALUATION
    gettimeofday(&t1, NULL);
#endif
    pthread_mutex_unlock(&lock);
#ifdef TIME_EVALUATION
    gettimeofday(&t2, NULL);
    addTimeMeasure(t1, t2, HANDLE_L);
#endif
    return ret;
  }

  ret = map.size();
  data->array_idx_ = ret;
  map.insert(std::make_pair(hash_value, ret));
  // LOG(ERROR) << "new " << (uint32_t)data->dump_type_ << "," << data->ToString() << "," << hash_value << "," << ret;
#ifdef TIME_EVALUATION
  gettimeofday(&t1, NULL);
#endif
  pthread_mutex_unlock(&lock);
#ifdef TIME_EVALUATION
  gettimeofday(&t2, NULL);
  addTimeMeasure(t1, t2, HANDLE_L);
#endif

#ifdef WRITE_FILE
  char* path = new char[150];
  std::hash<std::string> hash;
  sprintf(path, "/data/data/%s/revealer/%d_%s_%zu_%s.dat", package_name_.c_str(), pid_, random_prefix_.c_str(), hash(location),
      file);
  DumpItem* item = new DumpItem;
  item->path_ = std::string(path);
  item->item_ = data;
  // queue_.add(item);
#ifdef TIME_EVALUATION
  gettimeofday(&t1, NULL);
#endif
  ToDumpQueueUnblock(item);
#ifdef TIME_EVALUATION
  gettimeofday(&t2, NULL);
  addTimeMeasure(t1, t2, ADD_Q);
#endif
#else
  std::hash<std::string> hash;
  hash(location);
#endif

  return ret;
}

uint32_t Dumper::StringDump(std::string location, DumpString* s) {
  return GeneralDump(location, STRING_FILE, strings_, s, string_mutex_);
//  return GeneralDump(location, STRING_FILE, strings_, s, map_mutex_);
//  uint32_t ret;
//  pthread_mutex_lock(&dump_mutex_);
//  auto it = std::find_if(strings_.begin(), strings_.end(), CompareHelper<DumpString>(s));
//  if (it != strings_.end()) {
//    ret = it - strings_.begin();
//    delete s;
//    pthread_mutex_unlock(&dump_mutex_);
//    return ret;
//  }
//
//  ret = strings_.size();
//  s->array_idx_ = ret;
//  strings_.push_back(s);
//  pthread_mutex_unlock(&dump_mutex_);
//  // LOG(ERROR) << "writing string " << ds->string_ << "," << ds->string_length_;
//  #ifdef WRITE_FILE
//    char* path = new char[150];
//    std::hash<std::string> hash;
//    sprintf(path, "/data/data/%s/revealer/%d_%zu_%s.dat", package_name_.c_str(), pid_, hash(location),
//            STRING_FILE);
//    DumpItem* item = new DumpItem;
//    item->path_ = std::string(path);
//    item->item_ = s;
//    // queue_.add(item);
//    ToDumpQueueUnblock(item);
//  #else
//    std::hash<std::string> hash;
//    hash(location);
//  #endif
//
//  return ret;
}

uint16_t Dumper::TypeDump(std::string location, DumpType* type) {
  return GeneralDump(location, TYPE_FILE, types_, type, type_mutex_);
//  return GeneralDump(location, TYPE_FILE, types_, type, map_mutex_);
//  uint16_t ret;
//  pthread_mutex_lock(&dump_mutex_);
//  auto it = std::find_if(types_.begin(), types_.end(), CompareHelper<DumpType>(type));
//  if (it != types_.end()) {
//    ret = it - types_.begin();
//    delete type;
//    pthread_mutex_unlock(&dump_mutex_);
//    return ret;
//  }
//
//  ret = types_.size();
//  type->array_idx_ = ret;
//  types_.push_back(type);
//  pthread_mutex_unlock(&dump_mutex_);
//
//  #ifdef WRITE_FILE
//    char* path = new char[150];
//    std::hash<std::string> hash;
//    sprintf(path, "/data/data/%s/revealer/%d_%zu_%s.dat", package_name_.c_str(), pid_, hash(location),
//            TYPE_FILE);
//    DumpItem* item = new DumpItem;
//    item->path_ = std::string(path);
//    item->item_ = type;
//    // queue_.add(item);
//    ToDumpQueueUnblock(item);
//  #else
//    std::hash<std::string> hash;
//    hash(location);
//  #endif
//
//  return ret;
}

uint16_t Dumper::ProtoDump(std::string location, DumpProto* proto) {
  return GeneralDump(location, PROTO_FILE, protos_, proto, proto_mutex_);
//  return GeneralDump(location, PROTO_FILE, protos_, proto, map_mutex_);
//  uint16_t ret;
//  pthread_mutex_lock(&dump_mutex_);
//  auto it = std::find_if(protos_.begin(), protos_.end(), CompareHelper<DumpProto>(proto));
//  if (it != protos_.end()) {
//    ret = it - protos_.begin();
//    delete proto;
//    pthread_mutex_unlock(&dump_mutex_);
//    return ret;
//  }
//
//  ret = protos_.size();
//  proto->array_idx_ = ret;
//  protos_.push_back(proto);
//  pthread_mutex_unlock(&dump_mutex_);
//
//  #ifdef WRITE_FILE
//    char* path = new char[150];
//    std::hash<std::string> hash;
//    sprintf(path, "/data/data/%s/revealer/%d_%zu_%s.dat", package_name_.c_str(), pid_, hash(location),
//            PROTO_FILE);
//    DumpItem* item = new DumpItem;
//    item->path_ = std::string(path);
//    item->item_ = proto;
//    // queue_.add(item);
//    ToDumpQueueUnblock(item);
//  #else
//    std::hash<std::string> hash;
//    hash(location);
//  #endif
//
//  return ret;
}

uint32_t Dumper::FieldDump(std::string location, DumpField* field) {
  return GeneralDump(location, FIELD_FILE, fields_, field, field_mutex_);
//  return GeneralDump(location, FIELD_FILE, fields_, field, map_mutex_);
//  uint32_t ret;
//  pthread_mutex_lock(&dump_mutex_);
//  auto it = std::find_if(fields_.begin(), fields_.end(), CompareHelper<DumpField>(field));
//  if (it != fields_.end()) {
//    ret = it - fields_.begin();
//    delete field;
//    pthread_mutex_unlock(&dump_mutex_);
//    return ret;
//  }
//
//  ret = fields_.size();
//  field->array_idx_ = ret;
//  fields_.push_back(field);
//  pthread_mutex_unlock(&dump_mutex_);
//
//  #ifdef WRITE_FILE
//    char* path = new char[150];
//    std::hash<std::string> hash;
//    sprintf(path, "/data/data/%s/revealer/%d_%zu_%s.dat", package_name_.c_str(), pid_, hash(location),
//            FIELD_FILE);
//    DumpItem* item = new DumpItem;
//    item->path_ = std::string(path);
//    item->item_ = field;
//    // queue_.add(item);
//    ToDumpQueueUnblock(item);
//  #else
//    std::hash<std::string> hash;
//    hash(location);
//  #endif
//
//  return ret;
}

uint32_t Dumper::MethodDump(std::string location, DumpMethod* method) {
  return GeneralDump(location, METHOD_FILE, methods_, method, method_mutex_);
//  return GeneralDump(location, METHOD_FILE, methods_, method, map_mutex_);
//  uint32_t ret;
//  pthread_mutex_lock(&dump_mutex_);
//  auto it = std::find_if(methods_.begin(), methods_.end(), CompareHelper<DumpMethod>(method));
//  if (it != methods_.end()) {
//    ret = it - methods_.begin();
//    delete method;
//    pthread_mutex_unlock(&dump_mutex_);
//    return ret;
//  }
//
//  ret = methods_.size();
//  method->array_idx_ = ret;
//  methods_.push_back(method);
//  pthread_mutex_unlock(&dump_mutex_);
//
//  #ifdef WRITE_FILE
//    char* path = new char[150];
//    std::hash<std::string> hash;
//    sprintf(path, "/data/data/%s/revealer/%d_%zu_%s.dat", package_name_.c_str(), pid_, hash(location),
//            METHOD_FILE);
//    DumpItem* item = new DumpItem;
//    item->path_ = std::string(path);
//    item->item_ = method;
//    // queue_.add(item);
//    ToDumpQueueUnblock(item);
//  #else
//    std::hash<std::string> hash;
//    hash(location);
//  #endif
//
//  return ret;
}

uint16_t Dumper::ClassDump(std::string location, DumpClassDef* clz) {
  return GeneralDump(location, CLASS_FILE, classes_, clz, class_mutex_);
//  return GeneralDump(location, CLASS_FILE, classes_, clz, map_mutex_);
//  uint16_t ret;
//  pthread_mutex_lock(&dump_mutex_);
//  auto it = std::find_if(classes_.begin(), classes_.end(), CompareHelper<DumpClassDef>(clz));
//  if (it != classes_.end()) {
//    ret = it - classes_.begin();
//    delete clz;
//    pthread_mutex_unlock(&dump_mutex_);
//    return ret;
//  }
//
//  ret = classes_.size();
//  clz->array_idx_ = ret;
//  classes_.push_back(clz);
//  pthread_mutex_unlock(&dump_mutex_);
//
//  #ifdef WRITE_FILE
//    char* path = new char[150];
//    std::hash<std::string> hash;
//    sprintf(path, "/data/data/%s/revealer/%d_%zu_%s.dat", package_name_.c_str(), pid_, hash(location),
//            CLASS_FILE);
//    DumpItem* item = new DumpItem;
//    item->path_ = std::string(path);
//    item->item_ = clz;
//    // queue_.add(item);
//    ToDumpQueueUnblock(item);
//  #else
//    std::hash<std::string> hash;
//    hash(location);
//  #endif
//
//  return ret;
}

uint32_t Dumper::StaticValueDump(std::string location, DumpStaticValue* sv) {
  return GeneralDump(location, STATIC_VALUE_FILE, static_values_, sv, static_value_mutex_);
//  return GeneralDump(location, STATIC_VALUE_FILE, static_values_, sv, map_mutex_);
//  uint32_t ret;
//  pthread_mutex_lock(&dump_mutex_);
//  auto it = std::find_if(static_values_.begin(), static_values_.end(), CompareHelper<DumpStaticValue>(sv));
//  if (it != static_values_.end()) {
//    ret = it - static_values_.begin();
//    delete sv;
//    pthread_mutex_unlock(&dump_mutex_);
//    return ret;
//  }
//
//  ret = static_values_.size();
//  static_values_.push_back(sv);
//  pthread_mutex_unlock(&dump_mutex_);
//
//  #ifdef WRITE_FILE
//    char* path = new char[150];
//    std::hash<std::string> hash;
//    sprintf(path, "/data/data/%s/revealer/%d_%zu_%s.dat", package_name_.c_str(), pid_, hash(location),
//            STATIC_VALUE_FILE);
//    DumpItem* item = new DumpItem;
//    item->path_ = std::string(path);
//    item->item_ = sv;
//    // queue_.add(item);
//    ToDumpQueueUnblock(item);
//  #else
//    std::hash<std::string> hash;
//    hash(location);
//  #endif
//
//  return ret;
}

uint32_t Dumper::EncodedFieldDump(std::string location, DumpEncodedField* ef) {
  return GeneralDump(location, ENCODED_FIELD_FILE, encoded_fields_, ef, encoded_field_mutex_);
//  return GeneralDump(location, ENCODED_FIELD_FILE, encoded_fields_, ef, map_mutex_);
//  uint32_t ret;
//  pthread_mutex_lock(&dump_mutex_);
//  auto it = std::find_if(encoded_fields_.begin(), encoded_fields_.end(), CompareHelper<DumpEncodedField>(ef));
//  if (it != encoded_fields_.end()) {
//    ret = it - encoded_fields_.begin();
//    delete ef;
//    pthread_mutex_unlock(&dump_mutex_);
//    return ret;
//  }
//
//  ret = encoded_fields_.size();
//  encoded_fields_.push_back(ef);
//  pthread_mutex_unlock(&dump_mutex_);
//
//  #ifdef WRITE_FILE
//    char* path = new char[150];
//    std::hash<std::string> hash;
//    sprintf(path, "/data/data/%s/revealer/%d_%zu_%s.dat", package_name_.c_str(), pid_, hash(location),
//            ENCODED_FIELD_FILE);
//    DumpItem* item = new DumpItem;
//    item->path_ = std::string(path);
//    item->item_ = ef;
//    // queue_.add(item);
//    ToDumpQueueUnblock(item);
//  #else
//    std::hash<std::string> hash;
//    hash(location);
//  #endif
//
//  return ret;
}

uint32_t Dumper::EncodedMethodDump(std::string location, DumpEncodedMethod* em) {
  return GeneralDump(location, ENCODED_METHOD_FILE, encoded_methods_, em, encoded_method_mutex_);
//  return GeneralDump(location, ENCODED_METHOD_FILE, encoded_methods_, em, map_mutex_);
//  uint32_t ret;
//  pthread_mutex_lock(&dump_mutex_);
//  auto it = std::find_if(encoded_methods_.begin(), encoded_methods_.end(), CompareHelper<DumpEncodedMethod>(em));
//  if (it != encoded_methods_.end()) {
//    ret = it - encoded_methods_.begin();
//    delete em;
//    pthread_mutex_unlock(&dump_mutex_);
//    return ret;
//  }
//
//  ret = encoded_methods_.size();
//  encoded_methods_.push_back(em);
//  pthread_mutex_unlock(&dump_mutex_);
//
//  #ifdef WRITE_FILE
//    char* path = new char[150];
//    std::hash<std::string> hash;
//    sprintf(path, "/data/data/%s/revealer/%d_%zu_%s.dat", package_name_.c_str(), pid_, hash(location),
//            ENCODED_METHOD_FILE);
//    DumpItem* item = new DumpItem;
//    item->path_ = std::string(path);
//    item->item_ = em;
//    // queue_.add(item);
//    ToDumpQueueUnblock(item);
//  #else
//    std::hash<std::string> hash;
//    hash(location);
//  #endif
//
//  return ret;
}

uint32_t Dumper::CodeDump(std::string location, DumpCodeItem* code) {
  return GeneralDump(location, CODE_FILE, codes_, code, code_mutex_);
//  return GeneralDump(location, CODE_FILE, codes_, code, map_mutex_);
//  uint32_t ret;
//  pthread_mutex_lock(&dump_mutex_);
//  auto it = std::find_if(codes_.begin(), codes_.end(), CompareHelper<DumpCodeItem>(code));
//  if (it != codes_.end()) {
//    ret = it - codes_.begin();
//    delete code;
//    pthread_mutex_unlock(&dump_mutex_);
//    return ret;
//  }
//
//  ret = codes_.size();
//  codes_.push_back(code);
//  pthread_mutex_unlock(&dump_mutex_);
//
//  #ifdef WRITE_FILE
//    char* path = new char[150];
//    std::hash<std::string> hash;
//    sprintf(path, "/data/data/%s/revealer/%d_%zu_%s.dat", package_name_.c_str(), pid_, hash(location),
//            CODE_FILE);
//    DumpItem* item = new DumpItem;
//    item->path_ = std::string(path);
//    item->item_ = code;
//    // queue_.add(item);
//    ToDumpQueueUnblock(item);
//  #else
//    std::hash<std::string> hash;
//    hash(location);
//  #endif
//
//  return ret;
}

uint32_t Dumper::DumpStringFromDex(const DexFile& file, uint32_t string_idx) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DumpString* ds = new DumpString;
  const DexFile::StringId& id = file.GetStringId(string_idx);
//  LOG(ERROR) << "DumpStringFromDex " << file.GetLocation() << "," << string_idx << "," << id.string_data_off_;
  ds->string_ = std::string(file.GetStringDataAndUtf16Length(id, &ds->string_length_));
  return StringDump(file.GetLocation(), ds);
}

uint16_t Dumper::DumpTypeFromDex(const DexFile& file, uint16_t type_idx) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DumpType* dt = new DumpType;
  const DexFile::TypeId& class_type_id = file.GetTypeId(type_idx);
  dt->descriptor_idx_ = DumpStringFromDex(file, class_type_id.descriptor_idx_);
  return TypeDump(file.GetLocation(), dt);
}

uint32_t Dumper::DumpFieldFromDex(const DexFile& file, uint32_t field_idx) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DumpField* df = new DumpField;
  const DexFile::FieldId& field_id = file.GetFieldId(field_idx);
  df->class_idx_ = DumpTypeFromDex(file, field_id.class_idx_);
  df->type_idx_ = DumpTypeFromDex(file, field_id.type_idx_);
  df->name_idx_ = DumpStringFromDex(file, field_id.name_idx_);

  return FieldDump(file.GetLocation(), df);
}

uint32_t Dumper::DumpMethodFromDex(const DexFile& file, uint32_t method_idx) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DumpMethod* dm = new DumpMethod;
  const DexFile::MethodId& method_id = file.GetMethodId(method_idx);
  dm->class_idx_ = DumpTypeFromDex(file, method_id.class_idx_);

  DumpProto* proto = new DumpProto;
  const DexFile::ProtoId& proto_id = file.GetProtoId(method_id.proto_idx_);
  proto->shorty_idx_ = DumpStringFromDex(file, proto_id.shorty_idx_);
  proto->return_type_idx_ = DumpTypeFromDex(file, proto_id.return_type_idx_);

  if (proto_id.parameters_off_ != 0) {
    const DexFile::TypeList* list = file.GetProtoParameters(proto_id);
    uint32_t size = list->Size();
    uint32_t idx;
    for (idx = 0; idx < size; ++idx) {
      const DexFile::TypeItem& item = list->GetTypeItem(idx);
      proto->param_types_.push_back(DumpTypeFromDex(file, item.type_idx_));
    }
  }

  dm->proto_idx_ = ProtoDump(file.GetLocation(), proto);

//  const DexFile::TypeId& class_type_id = file.GetTypeId(method_id.class_idx_);
//  const DexFile::StringId& class_id = file.GetStringId(class_type_id.descriptor_idx_);
//  uint32_t strlen = 0;
//  const DexFile::StringId& name_id = file.GetStringId(method_id.name_idx_);
//  LOG(ERROR) << "DumpMethodFromDex " << file.GetLocation() << "," << method_idx
//      << "," << std::string(file.GetStringDataAndUtf16Length(name_id, &strlen))
//      << "," << std::string(file.GetStringDataAndUtf16Length(class_id, &strlen));

  dm->name_idx_ = DumpStringFromDex(file, method_id.name_idx_);

  return MethodDump(file.GetLocation(), dm);
}

uint32_t Dumper::DumpClassFromDex(const DexFile& file,
        const DexFile::ClassDef& class_def) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DumpClassDef* dcd = new DumpClassDef;

  dcd->class_idx_ = DumpTypeFromDex(file, class_def.class_idx_);
  dcd->access_flags_ = class_def.access_flags_ & 0x3ffff;

  if (class_def.superclass_idx_ != DexFile::kDexNoIndex16) {
    dcd->superclass_idx_ = DumpTypeFromDex(file, class_def.superclass_idx_);
  } else {
    dcd->superclass_idx_ = DexFile::kDexNoIndex16;
  }

  const DexFile::TypeList* interface_list = file.GetInterfacesList(class_def);
  if (interface_list) {
    uint32_t size = interface_list->Size();
    uint32_t idx;
    for (idx = 0; idx < size; ++idx) {
      const DexFile::TypeItem& item = interface_list->GetTypeItem(idx);
      dcd->interface_types_.push_back(DumpTypeFromDex(file, item.type_idx_));
    }
  }

  if (class_def.source_file_idx_ != DexFile::kDexNoIndex) {
    dcd->source_file_idx_ = DumpStringFromDex(file, class_def.source_file_idx_);
  } else {
    dcd->source_file_idx_ = DexFile::kDexNoIndex;
  }

  return ClassDump(file.GetLocation(), dcd);
}

uint32_t Dumper::DumpStaticValuesFromDex(const DexFile& file,
        const DexFile::ClassDef& class_def) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  if (class_def.static_values_off_ != 0) {
    DumpStaticValue* dsv = new DumpStaticValue;
    dsv->class_idx_ = DumpClassFromDex(file, class_def);

    const uint8_t* begin = file.GetEncodedStaticFieldValuesArray(class_def);
    uint32_t array_size = DecodeUnsignedLeb128(&begin);
    uint32_t idx;
    for (idx = 0; idx < array_size; ++idx) {
      uint8_t byte_value = *begin++;
      uint8_t value_arg = byte_value >> 5;
      uint8_t value_type = byte_value & 0x1f;
      size_t width = value_arg + 1;
      if (value_type == 0x1e || value_type == 0x1f) {
        width = 0;
      }

      uint8_t* new_value;
      uint16_t byte_size = width + 1;
      if (value_type == 0x17) {
        uint32_t static_idx = 0;
        if (width == 1) {
          static_idx = *(reinterpret_cast<const uint8_t*>(begin));
        } else if (width == 2) {
          static_idx = *(reinterpret_cast<const uint16_t*>(begin));
        } else if (width == 3) {
          static_idx = *(reinterpret_cast<const uint16_t*>(begin)) + ((*(reinterpret_cast<const uint8_t*>(begin + 2))) << 16);
        } else if (width == 4) {
          static_idx = *(reinterpret_cast<const uint32_t*>(begin));
        }

        uint32_t static_idx_new = DumpStringFromDex(file, static_idx);
        uint8_t byte_value_new = value_type | (3 << 5);
        byte_size = 5;
        new_value = new uint8_t[5];
        memcpy(new_value, &byte_value_new, 1);
        memcpy(&(new_value[1]), &static_idx_new, 4);
      } else if (value_type == 0x18) {
        uint32_t static_idx = 0;
        if (width == 1) {
          static_idx = *(reinterpret_cast<const uint8_t*>(begin));
        } else if (width == 2) {
          static_idx = *(reinterpret_cast<const uint16_t*>(begin));
        } else if (width == 3) {
          static_idx = *(reinterpret_cast<const uint16_t*>(begin)) + ((*(reinterpret_cast<const uint8_t*>(begin + 2))) << 16);
        } else if (width == 4) {
          static_idx = *(reinterpret_cast<const uint32_t*>(begin));
        }

        uint32_t staitc_idx_new = DumpTypeFromDex(file, static_idx);
        uint8_t byte_value_new = value_type | (3 << 5);
        byte_size = 5;
        new_value = new uint8_t[5];
        memcpy(new_value, &byte_value_new, 1);
        memcpy(&(new_value[1]), &staitc_idx_new, 4);
      } else if (value_type == 0x19 || value_type == 0x1b) {
        uint32_t static_idx = 0;
        if (width == 1) {
          static_idx = *(reinterpret_cast<const uint8_t*>(begin));
        } else if (width == 2) {
          static_idx = *(reinterpret_cast<const uint16_t*>(begin));
        } else if (width == 3) {
          static_idx = *(reinterpret_cast<const uint16_t*>(begin)) + ((*(reinterpret_cast<const uint8_t*>(begin + 2))) << 16);
        } else if (width == 4) {
          static_idx = *(reinterpret_cast<const uint32_t*>(begin));
        }

        uint32_t staitc_idx_new = DumpFieldFromDex(file, static_idx);
        uint8_t byte_value_new = value_type | (3 << 5);
        byte_size = 5;
        new_value = new uint8_t[5];
        memcpy(new_value, &byte_value_new, 1);
        memcpy(&(new_value[1]), &staitc_idx_new, 4);
      } else if (value_type == 0x1a) {
        uint32_t static_idx = 0;
        if (width == 1) {
          static_idx = *(reinterpret_cast<const uint8_t*>(begin));
        } else if (width == 2) {
          static_idx = *(reinterpret_cast<const uint16_t*>(begin));
        } else if (width == 3) {
          static_idx = *(reinterpret_cast<const uint16_t*>(begin)) + ((*(reinterpret_cast<const uint8_t*>(begin + 2))) << 16);
        } else if (width == 4) {
          static_idx = *(reinterpret_cast<const uint32_t*>(begin));
        }

        uint32_t staitc_idx_new = DumpMethodFromDex(file, static_idx);
        uint8_t byte_value_new = value_type | (3 << 5);
        byte_size = 5;
        new_value = new uint8_t[5];
        memcpy(new_value, &byte_value_new, 1);
        memcpy(&(new_value[1]), &staitc_idx_new, 4);
      } else {
        new_value = new uint8_t[width + 1];
        memcpy(new_value, &byte_value, 1);
        if (width > 0) {
          memcpy(&(new_value[1]), begin, width);
        }
      }
      dsv->values_.insert(std::pair<uint32_t, std::pair<uint16_t, uint8_t*>>(idx, std::pair<uint16_t, uint8_t*>(byte_size, new_value)));
      begin += width;
    }

    return StaticValueDump(file.GetLocation(), dsv);
  }
  return DexFile::kDexNoIndex;
}

uint32_t Dumper::DumpEncodedFieldFromDex(const DexFile& file, uint32_t field_idx,
        EncodedFieldType type, uint32_t access_flag) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DumpEncodedField* def = new DumpEncodedField;
  def->type_ = type;
  uint32_t idx = DumpFieldFromDex(file, field_idx);
  def->field_idx_ = idx;
  def->access_flags_ = access_flag & 0x3ffff;

  EncodedFieldDump(file.GetLocation(), def);

  return idx;
}

uint32_t Dumper::DumpEncodedMethodFromDex(const DexFile& file, uint32_t method_idx,
        EncodedMethodType type, uint32_t access_flag) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DumpEncodedMethod* dem = new DumpEncodedMethod;
  dem->type_ = type;
  uint32_t idx = DumpMethodFromDex(file, method_idx);
  dem->method_idx_ = idx;
  dem->access_flags_ = access_flag & 0x3ffff;

  EncodedMethodDump(file.GetLocation(), dem);

  return idx;
}

std::pair<uint32_t, uint32_t> Dumper::DumpImplicitEncodedMethod(ArtMethod* method,
    ShadowFrame& shadow_frame, uint16_t num_reg) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DumpEncodedMethod* dem = new DumpEncodedMethod;
  const DexFile* file = method->GetDexFile();
  uint32_t method_idx = method->GetDexMethodIndex();
  uint32_t idx = DumpMethodFromDex(*file, method_idx);
  dem->method_idx_ = idx;

  uint32_t current_cls_idx = 0xffffffff;

  auto pointer_size = Runtime::Current()->GetClassLinker()->GetImagePointerSize();
  mirror::Class* clz = method->GetDeclaringClass();
  if (clz->IsAbstract() || clz->IsInterface()) {
    if (!method->IsStatic()) {
//      std::string strtmp;
//      const char* destmp = clz->GetDescriptor(&strtmp);
//      LOG(ERROR) << "starting process abstract " << method->GetName() << " " << destmp;
      uint32_t shorty_len = 0;
      const char* shorty = method->GetShorty(&shorty_len);
      uint16_t this_idx = num_reg - 1;
      uint32_t tmp_idx = 1;
      for (tmp_idx = 1; tmp_idx < shorty_len; ++tmp_idx) {
        if (shorty[tmp_idx] == 'J' || shorty[tmp_idx] == 'D') {
          this_idx -= 2;
        } else {
          this_idx -= 1;
        }
      }
//      LOG(ERROR) << "shorty " << shorty << " num_reg " << num_reg << " this " << std::to_string((uint32_t)this_idx);
      mirror::Object* obj = shadow_frame.GetVRegReference(this_idx);
//      LOG(ERROR) << "obj got " << (obj ? "not null" : "null");
      if (obj) {
        mirror::Class* objclz = obj->GetClass();
//        LOG(ERROR) << "objclz got " << (objclz ? "not null" : "null");
        DumpString* string = new DumpString;
        const char* descriptor = objclz->GetDescriptor(&string->string_);
//        LOG(ERROR) << "objclz_name got " << (descriptor ? descriptor : "null");
        string->string_ = std::string(descriptor);
        string->string_length_ = string->string_.length();
        current_cls_idx = StringDump(file->GetLocation(), string);
//        LOG(ERROR) << "current_cls_idx " << current_cls_idx << " " << string->string_;
      }
    }
  }

  ArtMethod* found_method = method->GetDeclaringClass()->FindDirectMethod(clz->GetDexCache(), method_idx, pointer_size);
  if (!found_method) {
    found_method = method->GetDeclaringClass()->FindVirtualMethod(clz->GetDexCache(), method_idx, pointer_size);
    if (!found_method) {
      found_method = method->GetDeclaringClass()->FindVirtualMethod(clz->GetDexCache(), method_idx, pointer_size);
      if (found_method) {
        dem->type_ = VIRTUAL;
      }
    } else {
      dem->type_ = VIRTUAL;
    }
  } else {
    dem->type_ = DIRECT;
  }

  dem->method_idx_ = idx;
  dem->access_flags_ = method->GetAccessFlags() & 0x3ffff;

  EncodedMethodDump(file->GetLocation(), dem);

  return std::make_pair(idx, current_cls_idx);
}

#ifdef TIME_EVALUATION
void Dumper::addTimeMeasure(struct timeval s, struct timeval e, TimeMeasureType type) {
  uint64_t diff = (e.tv_sec - s.tv_sec) * 1000 + (e.tv_usec - s.tv_usec) / 1000;

  pthread_mutex_lock(&time_mutex_);

  switch (type) {
    case WRITE_F:
      measure_.write_file_ += diff;
      ++measure_.write_file_times_;
      break;
    case ADD_Q:
      measure_.add_queue_ += diff;
      ++measure_.add_queue_times_;
      break;
    case ARRAY_C:
      measure_.array_find_code_ += diff;
      ++measure_.array_find_code_times_;
      LOG(ERROR) << "time measure: " << measure_.write_file_ << "," << measure_.add_queue_
          << "," << measure_.array_find_code_ << "," << measure_.array_find_other_
          << "," << measure_.process_code_ << "," << measure_.handle_lock_;
      LOG(ERROR) << "time measure times: " << measure_.write_file_times_ << "," << measure_.add_queue_times_
                << "," << measure_.array_find_code_times_ << "," << measure_.array_find_other_times_
                << "," << measure_.process_code_times_ << "," << measure_.handle_lock_times_;
      break;
    case ARRAY_O:
      measure_.array_find_other_ += diff;
      ++measure_.array_find_other_times_;
      break;
    case PROCESS_C:
      measure_.process_code_ += diff;
      ++measure_.process_code_times_;
      break;
    case HANDLE_L:
      measure_.handle_lock_ += diff;
      ++measure_.handle_lock_times_;
      break;
  }

  pthread_mutex_unlock(&time_mutex_);
}
#endif

}  // namespace art
