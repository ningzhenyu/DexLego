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

#ifndef ART_RUNTIME_UNPACK_DUMP_HANDLE_H_
#define ART_RUNTIME_UNPACK_DUMP_HANDLE_H_

#include "unpack_dump.h"
#include <sys/time.h>

namespace art {


#ifdef PERFORMANCE_EVALUATION
#define ALLOW_TEMP_MEMORY(_fun_name_)                                               \
    MapAndList* root_map_and_list = nullptr;                                             \
    MapAndList* map_and_list = nullptr;                                        \
    uint8_t last_throw = 0xff;                                                      \
    DumpCodeItem* executed_code = nullptr;                            \
    if (shadow_frame.GetMethod()->ShouldManipulate()) {                             \
      gettimeofday(&start_time, NULL);      \
      executed_code = new DumpCodeItem;   \
      executed_code->registers_size_ = code_item->registers_size_; \
      executed_code->ins_size_ = code_item->ins_size_;  \
      executed_code->outs_size_ = code_item->outs_size_;  \
      root_map_and_list = new MapAndList(nullptr, 0);                                    \
      map_and_list = root_map_and_list;                                             \
      LOG(ERROR) << "method start " << start_time.tv_sec << " " << start_time.tv_usec;\
    }
#else
#define ALLOW_TEMP_MEMORY(_fun_name_)                                               \
    MapAndList* root_map_and_list = nullptr;                                             \
    MapAndList* map_and_list = nullptr;                                        \
    uint8_t last_throw = 0xff;                                                      \
    int32_t force_execution_offset = 0;                                  \
    DumpCodeItem* executed_code = nullptr;                                          \
    if (shadow_frame.GetMethod()->ShouldManipulate()) {                             \
      executed_code = new DumpCodeItem;   \
      executed_code->registers_size_ = code_item->registers_size_; \
      executed_code->ins_size_ = code_item->ins_size_;  \
      executed_code->outs_size_ = code_item->outs_size_;  \
      root_map_and_list = new MapAndList(nullptr, 0);                                    \
      map_and_list = root_map_and_list;                                             \
    }
#endif

#define FREE_TEMP_MEMORY()                        \
    delete root_map_and_list;

#define IGNORE_EXCEPTION (map_and_list && Dumper::Instance()->ForceExecution())

#define FORCE_PATH()                                                                       \
    int32_t force_ret = 0;                                                                 \
    if (map_and_list) {                                                                    \
      force_ret = Dumper::Instance()->GetForceBranch(shadow_frame.GetMethod(), dex_pc);    \
    }

#define HANDLE_INSTRUCTION(_count_)                                                         \
    if (map_and_list) {                                                                      \
      HandleInstruction(map_and_list, &code_item->insns_[dex_pc], shadow_frame, _count_);  \
    }

#define HANDLE_THROW_INSTRUCTION(_reg_)                                                     \
    if (map_and_list) {                                                                      \
      last_throw = _reg_;                                                                   \
    }

#define HANDLE_MOVE_EXCEPTION_INSTRUCTION(_reg_)                                            \
    if (map_and_list) {                                                                      \
      if (last_throw != 0xff) {                                                             \
        uint16_t* modified_inst = new uint16_t[2];                                          \
        modified_inst[0] = 0x02 | (_reg_ << 8);                                             \
        modified_inst[1] = static_cast<uint16_t>(last_throw);                               \
        HandleInstruction(map_and_list, modified_inst, shadow_frame, 2);                   \
      }                                                                                     \
    }

#define HANDLE_FILL_ARRAY_DATA_INSTRUCTION(_payload_)                                                            \
    if (map_and_list) {                                                                      \
      HandleFillArrayData(map_and_list, inst_data, shadow_frame, _payload_);   \
    }

#ifdef PERFORMANCE_EVALUATION
#define HANDLE_RETURN_INSTRUCTION(_count_)                                                      \
    if (map_and_list) {                                                                          \
      HandleInstruction(map_and_list, &code_item->insns_[dex_pc], shadow_frame, _count_);      \
      std::vector<uint16_t>* all_codes = new std::vector<uint16_t>;                             \
      CombineCodes(root_map_and_list, all_codes);                                               \
      HandleDump(shadow_frame, executed_code, all_codes);                                                       \
      FREE_TEMP_MEMORY();                                                                       \
      struct timeval end_time; \
      gettimeofday(&end_time, NULL);\
      LOG(ERROR) << "normal return method time consum " << (end_time.tv_sec - start_time.tv_sec) << " " << (end_time.tv_usec - start_time.tv_usec);\
    }
#else
#define HANDLE_RETURN_INSTRUCTION(_count_)                                                      \
    if (map_and_list) {                                                                          \
      HandleInstruction(map_and_list, &code_item->insns_[dex_pc], shadow_frame, _count_);      \
      std::vector<uint16_t>* all_codes = new std::vector<uint16_t>;                             \
      CombineCodes(root_map_and_list, all_codes);                                               \
      HandleDump(shadow_frame, executed_code, all_codes);                                                       \
      FREE_TEMP_MEMORY();                                                                       \
    }
#endif

#ifdef PERFORMANCE_EVALUATION
#define HANDLE_EXCEPTION_RETURN()                                                              \
    if (map_and_list) {                                                                      \
      uint16_t* modified_inst = new uint16_t[1];                      \
      modified_inst[0] = 0xe;                                                   \
      HandleInstruction(map_and_list, modified_inst, shadow_frame, 1);   \
      delete modified_inst;               \
      std::vector<uint16_t>* all_codes = new std::vector<uint16_t>;                                  \
      CombineCodes(root_map_and_list, all_codes);                             \
      HandleDump(shadow_frame, executed_code, all_codes);                           \
      FREE_TEMP_MEMORY();                                                                      \
      struct timeval end_time; \
      gettimeofday(&end_time, NULL);\
      LOG(ERROR) << "exception return method time consum " << (end_time.tv_sec - start_time.tv_sec) << " " << (end_time.tv_usec - start_time.tv_usec);\
    }
#else
#define HANDLE_EXCEPTION_RETURN()                                                              \
    if (map_and_list) {                                                                      \
      uint16_t* modified_inst = new uint16_t[1];                      \
      modified_inst[0] = 0xe;                                                   \
      HandleInstruction(map_and_list, modified_inst, shadow_frame, 1);   \
      delete modified_inst;               \
      std::vector<uint16_t>* all_codes = new std::vector<uint16_t>;                                  \
      CombineCodes(root_map_and_list, all_codes);                             \
      HandleDump(shadow_frame, executed_code, all_codes);                           \
      FREE_TEMP_MEMORY();                                                                      \
    }
#endif

#ifdef PERFORMANCE_EVALUATION
#define HANDLE_SPECIAL_RETURN_INSTRUCTION(_count_)                                     \
    if (map_and_list) {                                                                      \
      uint16_t* modified_inst = new uint16_t[_count_];                      \
      memcpy(modified_inst, &code_item->insns_[dex_pc], _count_ * 2);           \
      modified_inst[0] = 0xe;                                                   \
      HandleInstruction(map_and_list, modified_inst, shadow_frame, _count_);   \
      delete modified_inst;               \
      std::vector<uint16_t>* all_codes = new std::vector<uint16_t>;                                  \
      CombineCodes(root_map_and_list, all_codes);                             \
      HandleDump(shadow_frame, executed_code, all_codes);                           \
      FREE_TEMP_MEMORY();                                               \
      struct timeval end_time; \
      gettimeofday(&end_time, NULL);\
      LOG(ERROR) << "special return method time consum " << (end_time.tv_sec - start_time.tv_sec) << " " << (end_time.tv_usec - start_time.tv_usec);\
    }
#else
#define HANDLE_SPECIAL_RETURN_INSTRUCTION(_count_)                                     \
    if (map_and_list) {                                                                      \
      uint16_t* modified_inst = new uint16_t[_count_];                      \
      memcpy(modified_inst, &code_item->insns_[dex_pc], _count_ * 2);           \
      modified_inst[0] = 0xe;                                                   \
      HandleInstruction(map_and_list, modified_inst, shadow_frame, _count_);   \
      delete modified_inst;               \
      std::vector<uint16_t>* all_codes = new std::vector<uint16_t>;                                  \
      CombineCodes(root_map_and_list, all_codes);                             \
      HandleDump(shadow_frame, executed_code, all_codes);                           \
      FREE_TEMP_MEMORY();                                               \
    }
#endif

#define HANDLE_GOTO_INSTRUCTION(_offset_)                                                      \
    if (map_and_list) {                                                                      \
      HandleGoto(map_and_list, shadow_frame, _offset_);                         \
    }

#define HANDLE_SWITCH_INSTRUCTION(_key_, _offset_)                                             \
    if (map_and_list) {                                                                      \
      HandleSwitch(map_and_list, inst_data, shadow_frame, _offset_, _key_);                                                     \
    }

#define HANDLE_IF_INSTRUCTION(_offset_)                                                        \
    if (map_and_list) {                                                                      \
      HandleIf(map_and_list, inst_data, shadow_frame, _offset_);           \
    }

#define HANDLE_INSTRUCTION_ABOUT_STRING(_str_idx_)                                    \
    if (map_and_list) {                                                                      \
      uint16_t* modified_inst = HandleString(inst_data, _str_idx_, shadow_frame);                                   \
      HandleInstruction(map_and_list, modified_inst, shadow_frame, 3);   \
      delete modified_inst;                   \
    }

#define HANDLE_INSTRUCTION_ABOUT_TYPE(_type_idx_, _count_)                                     \
    if (map_and_list) {                                                                      \
      uint16_t* modified_inst = HandleType(&code_item->insns_[dex_pc], _type_idx_, shadow_frame, _count_);                                    \
      HandleInstruction(map_and_list, modified_inst, shadow_frame, _count_);   \
      delete modified_inst;               \
    }

#define HANDLE_INSTRUCTION_ABOUT_FIELD(f, _is_get_) \
    if (inst_list) {                                                                      \
      uint16_t* modified_inst;                                                              \
      if (_is_get_) {                                                                          \
        modified_inst = HandleFieldGet<find_type, field_type, do_access_check>(inst_list,    \
                          shadow_frame, 2, f, is_static);                                           \
      } else {                                                                                 \
        modified_inst = HandleFieldPut<find_type, field_type, do_access_check>(inst_list,           \
                          shadow_frame, 2, f, is_static);                                           \
      }                                                                                        \
      HandleInstruction(map_and_list, modified_inst, shadow_frame, 2);         \
      delete modified_inst;                       \
    }

#define HANDLE_INSTRUCTION_ABOUT_FIELD_QUICK(f, _is_get_)                           \
    if (inst_list) {                                                                      \
      uint16_t* modified_inst;                                                              \
      if (_is_get_) {                                                                          \
        modified_inst = HandleFieldGetQuick<field_type>(inst_list, shadow_frame, f, 2);      \
      } else {                                                                                 \
        modified_inst = HandleFieldPutQuick<field_type>(inst_list, shadow_frame, f, 2);      \
      }                                                                                        \
      HandleInstruction(map_and_list, modified_inst, shadow_frame, 2);         \
      delete modified_inst;                       \
    }

#define HANDLE_INSTRUCTION_ABOUT_INVOKE()                 \
    if (inst_list) {                                                                      \
      uint16_t* modified_inst;                                                              \
      modified_inst = HandleInvoke<type, is_range, do_access_check>(inst_list, shadow_frame,  \
          called_method, 3);              \
      uint16_t count = modified_inst[0];                                            \
      HandleInstruction(map_and_list, &(modified_inst[1]), shadow_frame, count);         \
      delete modified_inst;                       \
    }

#define HANDLE_INSTRUCTION_ABOUT_INVOKE_QUICK()                                      \
    if (inst_list) {                                                                      \
      uint16_t* modified_inst;                                                              \
      modified_inst = HandleInvokeQuick<is_range>(inst_list, shadow_frame, called_method, 3);          \
      uint16_t count = modified_inst[0];                                            \
      HandleInstruction(map_and_list, &(modified_inst[1]), shadow_frame, count);         \
      delete modified_inst;                       \
    }

struct DumpSwitchTable {
  std::map<int32_t, int32_t> target_map_;
};

struct DumpFillArrayData {
  uint16_t element_width_;
  uint32_t element_count_;
  std::vector<uint8_t> datas_;
};

#define CODE_NO_INDEX 0xffffffff

struct MapAndList {
  std::vector<uint16_t>* code_list;
//  CodeMap* code_map;
  std::vector<uint32_t>* code_map_key;
  std::vector<uint32_t>* code_map_value;
  std::map<uint32_t, DumpSwitchTable*>* switch_table_map;
  std::map<uint32_t, DumpFillArrayData*>* fill_array_data_map;
  std::vector<MapAndList*>* childs;
  uint32_t start_pos;
  uint32_t end_pos;
  uint32_t prev_ins_pos;
  MapAndList* parent;
  bool inited;

  MapAndList() {
//    code_list = nullptr;
//    // code_map = nullptr;
//    code_map_key = nullptr;
//    code_map_value = nullptr;
//    switch_table_map = nullptr;
//    fill_array_data_map = nullptr;
//    childs = nullptr;
//    parent = nullptr;
//    inited = false;
    LOG(FATAL) << "unwanted constructor " << this;
  }

  MapAndList(MapAndList* p, uint32_t s) {
    init(p, s);
    // LOG(ERROR) << "constructor " << this;
  }

  void init(MapAndList* p, uint32_t s) {
    inited = true;
    code_list = new std::vector<uint16_t>;
    // code_map = new CodeMap;
    code_map_key = new std::vector<uint32_t>;
    code_map_value = new std::vector<uint32_t>;
    switch_table_map = new std::map<uint32_t, DumpSwitchTable*>;
    fill_array_data_map = new std::map<uint32_t, DumpFillArrayData*>;
    childs = new std::vector<MapAndList*>;
    start_pos = s;
    prev_ins_pos = 0;
    parent = p;
    if (p) {
      end_pos = 0xffffffff;
      p->childs->push_back(this);
    }
  }

  uint32_t FindCodeInCodeMap(uint32_t key) {
    // LOG(ERROR) << "FindCodeInCodeMap " << this << " " << key << " "
    //     << code_map_key->size() << " " << code_map_value->size();
    // return code_map.find(key);
    auto it = std::find(code_map_key->begin(), code_map_key->end(), key);
    return it == code_map_key->end() ? CODE_NO_INDEX : code_map_value->at(it - code_map_key->begin());
  }

  void PushCodeToCodeMap(uint32_t key, uint32_t value) {
//    LOG(ERROR) << "PushCodeToCodeMap " << this << " " << key << " " << value << " "
//        << code_map_key->size() << " " << code_map_value->size();;
    // code_map->insert(std::make_pair(key, value));
    code_map_key->push_back(key);
    code_map_value->push_back(value);
  }

  ~MapAndList() {
    if (inited) {
//      LOG(ERROR) << "destructor " << this;
    } else {
      LOG(FATAL) << "unwanted destructor " << this;
    }

    if (code_list) {
      code_list->clear();
      delete code_list;
      code_list = nullptr;
    }

//    if (code_map) {
//      code_map->clear();
//      delete code_map;
//      code_map = nullptr;
//    }

    if (code_map_key) {
      code_map_key->clear();
      delete code_map_key;
      code_map_key = nullptr;
    }

    if (code_map_value) {
      code_map_value->clear();
      delete code_map_value;
      code_map_value = nullptr;
    }

    if (switch_table_map) {
      for (auto iter : *switch_table_map) {
        delete iter.second;
      }
      switch_table_map->clear();
      delete switch_table_map;
      switch_table_map = nullptr;
    }

    if (fill_array_data_map) {
      for (auto iter : *fill_array_data_map) {
        delete iter.second;
      }
      fill_array_data_map->clear();
      delete fill_array_data_map;
      fill_array_data_map = nullptr;
    }

    if (childs) {
      for (auto sub_list : *childs) {
        delete sub_list;
      }
      childs->clear();
      delete childs;
      childs = nullptr;
    }
  }
};

#define NOPS_BEFORE_EACH_INST 8

static inline int32_t GetOffsetForPos(MapAndList*& list, uint32_t dex_pc, int32_t offset) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  uint32_t idx = list->FindCodeInCodeMap(dex_pc + offset);
  if (idx != CODE_NO_INDEX) {
    return idx;
  } else {
    return list->code_list->size() + NOPS_BEFORE_EACH_INST;
  }
}

static inline bool IsSameInstruction(const uint16_t* code, std::vector<uint16_t>* list, uint32_t mapped_pos, uint32_t count) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  bool ret = true;
  uint32_t idx = 0;
  for (idx = 0; idx < count; ++idx) {
    if (code[idx] != list->at(mapped_pos + idx)) {
      return false;
    }
  }
  return ret;
}

#define DUMP_GOTO_INSTRUCTION 0x2A

static inline bool IsSameGotoInstruction(MapAndList*& list, uint32_t mapped_pos,
    int32_t offset) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  if (list->code_list->at(mapped_pos) != DUMP_GOTO_INSTRUCTION) {
    return false;
  }

  int32_t old_offset = static_cast<int32_t>(list->code_list->at(mapped_pos + 1)
      | (list->code_list->at(mapped_pos + 2) << 16));

  return old_offset == offset;
}

static inline bool IsSameFillArrayDataInstruction(MapAndList*& list, uint16_t instruction,
    uint32_t dex_pc, uint32_t mapped_pos,
    const Instruction::ArrayDataPayload* payload) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  if (list->code_list->at(mapped_pos) != instruction) {
    return false;
  }

  DumpFillArrayData* data = list->fill_array_data_map->find(dex_pc)->second;
  if (data->element_count_ != payload->element_count || data->element_width_ != payload->element_width) {
    return false;
  }
  uint32_t total = data->element_count_ * data->element_width_;
  uint32_t idx;
  for (idx = 0; idx < total; ++idx) {
    if (data->datas_.at(idx) != payload->data[idx]) {
      return false;
    }
  }

  return true;
}

#define DUMP_SWITCH_INSTRUCTION 0x2C

static inline bool IsSameSwitchInstruction(MapAndList*& list, uint16_t instruction,
    uint32_t dex_pc, uint32_t mapped_pos, int32_t offset, int32_t key,
    bool is_default) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  if (list->code_list->at(mapped_pos) != instruction) {
    return false;
  }

  if (is_default) {
    int32_t offset_fix = offset - 3;
    int32_t mapped_pos_fix = mapped_pos + 3;
    if ((*(list->code_list))[mapped_pos_fix] != DUMP_GOTO_INSTRUCTION) {
      // modify the goto instruction added while entering the switch for the first time.
      (*(list->code_list))[mapped_pos_fix] = DUMP_GOTO_INSTRUCTION;
      (*(list->code_list))[mapped_pos_fix + 1] = static_cast<uint16_t>(offset_fix & 0xffff);
      (*(list->code_list))[mapped_pos_fix + 2] = static_cast<uint16_t>((offset_fix & 0xffff0000) >> 16);
      return true;
    } else {
      return IsSameGotoInstruction(list, mapped_pos_fix, offset_fix);
    }
  } else {
    DumpSwitchTable* switch_table = list->switch_table_map->find(dex_pc)->second;
    auto iter = switch_table->target_map_.find(key);
    if (iter != switch_table->target_map_.end()) {
      return iter->second == offset;
    } else {
      switch_table->target_map_.insert(std::pair<int32_t, int32_t>(key, offset));
      return true;
    }
  }
}

static inline bool IsSameIfInstruction(MapAndList*& list, uint16_t instruction,
    uint32_t mapped_pos, int32_t offset, bool is_else) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  if (list->code_list->at(mapped_pos) != instruction) {
    return false;
  }

  int32_t offset_fix;
  int32_t mapped_pos_fix;
  if (is_else) {
    offset_fix = offset - 2;
    mapped_pos_fix = mapped_pos + 2;
  } else {
    offset_fix = offset - 5;
    mapped_pos_fix = mapped_pos + 5;
  }

  if ((*(list->code_list))[mapped_pos_fix] != DUMP_GOTO_INSTRUCTION) {
    // we haven't met this case before
    (*(list->code_list))[mapped_pos_fix] = DUMP_GOTO_INSTRUCTION;
    (*(list->code_list))[mapped_pos_fix + 1] = static_cast<uint16_t>(offset_fix & 0xffff);
    (*(list->code_list))[mapped_pos_fix + 2] = static_cast<uint16_t>((offset_fix & 0xffff0000) >> 16);
    return true;
  } else {
    return IsSameGotoInstruction(list, mapped_pos_fix, offset_fix);
  }
}

static inline void PushInstructionToList(MapAndList*& list, const uint16_t* code,
    uint32_t dex_pc, uint32_t count, bool is_move_result) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  uint32_t idx;
  if (!is_move_result) {
    // Add nops before each instruction, just for convenience of combine branches
    for (idx = 0; idx < NOPS_BEFORE_EACH_INST; ++idx) {
      list->code_list->push_back(0);
    }
  }  // Add nops before move-result instruction is not allowed.

  list->PushCodeToCodeMap(dex_pc, list->code_list->size());

  for (idx = 0; idx < count; ++idx) {
    list->code_list->push_back(code[idx]);
  }

  list->prev_ins_pos = list->code_list->size();
}

static inline void PushGotoInstructionToList(MapAndList*& list, uint32_t dex_pc, int32_t offset) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  uint32_t idx;
  // Add nops before each instruction, just for convenience of combine branches
  for (idx = 0; idx < NOPS_BEFORE_EACH_INST; ++idx) {
    list->code_list->push_back(0);
  }

  list->PushCodeToCodeMap(dex_pc, list->code_list->size());
  list->prev_ins_pos = list->code_list->size() + offset;

  list->code_list->push_back(DUMP_GOTO_INSTRUCTION);
  list->code_list->push_back(static_cast<uint16_t>(offset & 0xffff));
  list->code_list->push_back(static_cast<uint16_t>((offset & 0xffff0000) >> 16));
}

static inline void PushSwitchInstructionToList(MapAndList*& list, uint16_t instruction,
    uint32_t dex_pc, int32_t offset, int32_t key, bool is_default) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  uint32_t idx;
  // Add nops before each instruction, just for convenience of combine branches
  for (idx = 0; idx < NOPS_BEFORE_EACH_INST; ++idx) {
    list->code_list->push_back(0);
  }

  list->PushCodeToCodeMap(dex_pc, list->code_list->size());
  list->prev_ins_pos = list->code_list->size() + offset;

  list->code_list->push_back(instruction);
  // reserve space for table offset
  list->code_list->push_back(0);
  list->code_list->push_back(0);

  if (is_default) {
    int32_t offset_fix = offset - 3;
    list->code_list->push_back(DUMP_GOTO_INSTRUCTION);
    list->code_list->push_back(static_cast<uint16_t>(offset_fix & 0xffff));
    list->code_list->push_back(static_cast<uint16_t>((offset_fix & 0xffff0000) >> 16));
  } else {
    // Add a dummy default case. If we met a default case later, we will fix this.
    list->code_list->push_back(0x00);
    list->code_list->push_back(0x00);
    list->code_list->push_back(0x0e);
  }

  // create switch table for this switch
  DumpSwitchTable* switch_table = new DumpSwitchTable;
  switch_table->target_map_.insert(std::pair<int32_t, int32_t>(key, offset));
  // LOG(ERROR) << "insert into switch table " << list->code_list->size() - 6 << " " << key << " " << offset;
  list->switch_table_map->insert(std::pair<uint32_t, DumpSwitchTable*>(dex_pc, switch_table));
}

static inline void PushFillArrayDataInstructionToList(MapAndList*& list, uint16_t instruction,
    uint32_t dex_pc, const Instruction::ArrayDataPayload* payload) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  uint32_t idx;
  // Add nops before each instruction, just for convenience of combine branches
  for (idx = 0; idx < NOPS_BEFORE_EACH_INST; ++idx) {
    list->code_list->push_back(0);
  }

  list->PushCodeToCodeMap(dex_pc, list->code_list->size());
  list->prev_ins_pos = list->code_list->size();

  list->code_list->push_back(instruction);
  // reserve space for data offset
  list->code_list->push_back(0);
  list->code_list->push_back(0);

  // create switch table for this switch
  DumpFillArrayData* data = new DumpFillArrayData;
  data->element_width_ = payload->element_width;
  data->element_count_ = payload->element_count;
  uint32_t total_size = data->element_width_ * data->element_count_;

  for (idx = 0; idx < total_size; ++idx) {
    // LOG(ERROR) << "push " << payload->data[idx];
    data->datas_.push_back(payload->data[idx]);
  }
  list->fill_array_data_map->insert(std::pair<uint32_t, DumpFillArrayData*>(dex_pc, data));
}

static inline void PushIfInstructionToList(MapAndList*& list, uint16_t instruction,
    uint32_t dex_pc, int32_t offset, bool is_else) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  uint32_t idx;
  // Add nops before each instruction, just for convenience of combine branches
  for (idx = 0; idx < NOPS_BEFORE_EACH_INST; ++idx) {
    list->code_list->push_back(0);
  }

  list->PushCodeToCodeMap(dex_pc, list->code_list->size());
  list->prev_ins_pos = list->code_list->size() + offset;

  list->code_list->push_back(instruction);
  // offset for the if case
  list->code_list->push_back(5);

  if (is_else) {
    // sub the length of this instruction
    offset = offset - 2;
    // Add the else case
    list->code_list->push_back(DUMP_GOTO_INSTRUCTION);
    list->code_list->push_back(static_cast<uint16_t>(offset & 0xffff));
    list->code_list->push_back(static_cast<uint16_t>((offset & 0xffff0000) >> 16));

    // Add a dummy if case. If we met a if case later, we will fix this.
    list->code_list->push_back(0x00);
    list->code_list->push_back(0x00);
    list->code_list->push_back(0x0e);
  } else {
    // Add a dummy else case. If we met a else case later, we will fix this.
    list->code_list->push_back(0x00);
    list->code_list->push_back(0x00);
    list->code_list->push_back(0x0e);

    // sub the length of this instruction and the length of else case
    offset = offset - 5;
    // Add the if case
    list->code_list->push_back(DUMP_GOTO_INSTRUCTION);
    list->code_list->push_back(static_cast<uint16_t>(offset & 0xffff));
    list->code_list->push_back(static_cast<uint16_t>((offset & 0xffff0000) >> 16));
  }
}
static inline void CombineCodes(MapAndList* list, std::vector<uint16_t>* codes) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  // LOG(ERROR) << "CombineCodes begin";
  codes->push_back(static_cast<uint16_t>(list->start_pos & 0xffff));
  codes->push_back(static_cast<uint16_t>((list->start_pos & 0xffff0000) >> 16));
  if (!list->parent) {
    list->end_pos = 0;
  }

  codes->push_back(static_cast<uint16_t>(list->end_pos & 0xffff));
  codes->push_back(static_cast<uint16_t>((list->end_pos & 0xffff0000) >> 16));

  uint32_t code_size = list->code_list->size();
  codes->push_back(static_cast<uint16_t>(code_size & 0xffff));
  codes->push_back(static_cast<uint16_t>((code_size & 0xffff0000) >> 16));
  for (uint16_t code : *list->code_list) {
    codes->push_back(code);
  }

  uint32_t map_size = list->code_map_key->size() * 2;
  codes->push_back(static_cast<uint16_t>(map_size & 0xffff));
  codes->push_back(static_cast<uint16_t>((map_size & 0xffff0000) >> 16));
//  for (auto iter : *list->code_map_key) {
//    codes->push_back(static_cast<uint16_t>(iter.first & 0xffff));
//    codes->push_back(static_cast<uint16_t>((iter.first & 0xffff0000) >> 16));
//    codes->push_back(static_cast<uint16_t>(iter.second & 0xffff));
//    codes->push_back(static_cast<uint16_t>((iter.second & 0xffff0000) >> 16));
//  }
  for (uint32_t i = 0; i < list->code_map_key->size(); ++i) {
    uint32_t key = list->code_map_key->at(i);
    uint32_t value = list->code_map_value->at(i);
    codes->push_back(static_cast<uint16_t>(key & 0xffff));
    codes->push_back(static_cast<uint16_t>((key & 0xffff0000) >> 16));
    codes->push_back(static_cast<uint16_t>(value & 0xffff));
    codes->push_back(static_cast<uint16_t>((value & 0xffff0000) >> 16));
  }

  uint32_t table_count = list->switch_table_map->size();
  codes->push_back(static_cast<uint16_t>(table_count & 0xffff));
  codes->push_back(static_cast<uint16_t>((table_count & 0xffff0000) >> 16));

  for (auto table_iter : *list->switch_table_map) {
    uint32_t switch_dex_pc = table_iter.first;
    uint32_t switch_pos_in_list = list->FindCodeInCodeMap(switch_dex_pc);
    // uint32_t table_offset = list->code_list->size() - switch_pos_in_list;
    // (*(list->code_list))[switch_pos_in_list + 1] = table_offset & 0xffff;
    // (*(list->code_list))[switch_pos_in_list + 2] = static_cast<uint16_t>((table_offset & 0xffff0000) >> 16);

    codes->push_back(static_cast<uint16_t>(switch_pos_in_list & 0xffff));
    codes->push_back(static_cast<uint16_t>((switch_pos_in_list & 0xffff0000) >> 16));

    DumpSwitchTable* switch_table = table_iter.second;
    uint16_t ss = switch_table->target_map_.size();

    codes->push_back(0x0200);
    codes->push_back(static_cast<uint16_t>(ss));
    // LOG(ERROR) << switch_dex_pc << " " << switch_pos_in_list << " " << switch_table->target_map_.size();
    for (auto iter : switch_table->target_map_) {
      int32_t key = iter.first;
      codes->push_back(key & 0xffff);
      codes->push_back(static_cast<uint16_t>((key & 0xffff0000) >> 16));
      // LOG(ERROR) << "CombineCodes push key " << switch_pos_in_list << " " << key;
    }
    for (auto iter : switch_table->target_map_) {
      int32_t target = iter.second;
      codes->push_back(target & 0xffff);
      codes->push_back(static_cast<uint16_t>((target & 0xffff0000) >> 16));
      // LOG(ERROR) << "CombineCodes push target " << switch_pos_in_list << " " << target;
    }
  }

  uint32_t map_count = list->fill_array_data_map->size();
  codes->push_back(static_cast<uint16_t>(map_count & 0xffff));
  codes->push_back(static_cast<uint16_t>((map_count & 0xffff0000) >> 16));

  for (auto table_iter : *list->fill_array_data_map) {
    uint32_t fill_dex_pc = table_iter.first;
    uint32_t fill_pos_in_list = list->FindCodeInCodeMap(fill_dex_pc);

    codes->push_back(static_cast<uint16_t>(fill_pos_in_list & 0xffff));
    codes->push_back(static_cast<uint16_t>((fill_pos_in_list & 0xffff0000) >> 16));

    DumpFillArrayData* fill_array_data = table_iter.second;
    uint32_t data_size = fill_array_data->datas_.size();

    codes->push_back(0x0300);
    codes->push_back(fill_array_data->element_width_);
    codes->push_back(static_cast<uint16_t>(fill_array_data->element_count_ & 0xffff));
    codes->push_back(static_cast<uint16_t>((fill_array_data->element_count_ & 0xffff0000) >> 16));

    uint32_t idx = 0;
    while (idx < data_size) {
      if (idx < data_size - 1) {
        uint8_t v1 = fill_array_data->datas_.at(idx);
        uint8_t v2 = fill_array_data->datas_.at(idx + 1);
        uint16_t v = static_cast<uint16_t>(static_cast<uint16_t>(v1) | (static_cast<uint16_t>(v2) << 8));
        codes->push_back(v);
        // LOG(ERROR) << "push array data " << v << " " << v1 << " " << v2;
      } else {
        uint8_t v1 = fill_array_data->datas_.at(idx);
        uint16_t v = static_cast<uint16_t>(v1);
        codes->push_back(v);
        // LOG(ERROR) << "push array data " << v << " " << v1;
      }
      idx += 2;
    }
  }

  uint32_t child_count = list->childs->size();
  codes->push_back(static_cast<uint16_t>(child_count & 0xffff));
  codes->push_back(static_cast<uint16_t>((child_count & 0xffff0000) >> 16));

  for (auto sub_list : *list->childs) {
    CombineCodes(sub_list, codes);
  }
}

static inline void HandleDump(ShadowFrame& shadow_frame, DumpCodeItem* executed_code,
        std::vector<uint16_t>* codes) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  uint32_t code_size = codes->size();

  // TODO fix this
  // executed_code->tries_size_ = 0;
  // executed_code->debug_info_off_ = 0;

  executed_code->insns_size_in_code_units_ = code_size;
  executed_code->insns_ = new uint16_t[code_size];

  uint32_t idx = 0;
  for (uint16_t i : *codes) {
    memcpy(&(executed_code->insns_[idx++]), &i, 2);
  }
  // LOG(ERROR) << "HandleDump " << code_size << " " << codes->size() << " "
  //     << executed_code->insns_size_in_code_units_ << " " << idx;

  ArtMethod* method = shadow_frame.GetMethod();
  std::pair<uint32_t, uint32_t> ret = Dumper::Instance()->DumpImplicitEncodedMethod(method, shadow_frame, executed_code->registers_size_);
  executed_code->method_idx_ = ret.first;
  executed_code->current_clz_name_idx_ = ret.second;

  Dumper::Instance()->CodeDump(method->GetDexFile()->GetLocation(), executed_code);
}
#ifdef TIME_EVALUATION
#define TIME_MEASURE_BEGIN \
  struct timeval t1, t2;   \
  gettimeofday(&t1, NULL);

#define TIME_MEASURE_END \
  gettimeofday(&t2, NULL); \
  Dumper::Instance()->addTimeMeasure(t1, t2, PROCESS_C);
#else
#define TIME_MEASURE_BEGIN
#define TIME_MEASURE_END

#endif

template <typename K, typename V>
class CompareHelper2 {
  public:
    explicit CompareHelper2(const K obj) : instance(obj) {}
    bool operator() (const std::pair<K, V> rhs) const { return instance == rhs.first; }
  private:
    const K instance;
};

static inline void HandleInstruction(MapAndList*& list, const uint16_t* code,
    const ShadowFrame& shadow_frame, uint32_t count) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  TIME_MEASURE_BEGIN
  uint32_t dex_pc = shadow_frame.GetDexPC();

  uint8_t opcode = code[0] & 0xff;
  bool is_move_result_ins = opcode >= 0x0a && opcode <= 0x0c;

  // LOG(ERROR) << "HandleInstruction list empty " << (list == nullptr);
  // LOG(ERROR) << "HandleInstruction map_and_list empty " << (map_and_list == nullptr) << " " << dex_pc;
  // LOG(ERROR) << "HandleInstruction code_map empty " << (map_and_list->code_map == nullptr);
  // LOG(ERROR) << "HandleInstruction code_map size " << (map_and_list->code_map->size());
  // auto iter = list->FindCodeInCodeMap(dex_pc);
  // auto iter = std::find_if(list->code_map->begin(), list->code_map->end(), CompareHelper2<uint32_t, uint32_t>(dex_pc));
  uint32_t index = list->FindCodeInCodeMap(dex_pc);

  if (is_move_result_ins) {
    // LOG(ERROR) << "HandleInstruction MOVE_RESULT";c
    if (index != CODE_NO_INDEX) {
      if (list->prev_ins_pos == list->code_list->size()) {
        PushInstructionToList(list, code, dex_pc, count, true);
      } else {
        if (!IsSameInstruction(code, list->code_list, index, count)) {
          // different instructions in same pos, switch to a hack branch
          MapAndList* branch_list = new MapAndList(list, index);
          PushInstructionToList(branch_list, code, dex_pc, count, false);
          list = branch_list;
        }
      }
    } else {
      PushInstructionToList(list, code, dex_pc, count, true);
    }
    TIME_MEASURE_END
    return;
  }

  if (index != CODE_NO_INDEX) {
    if (list->prev_ins_pos == list->code_list->size()) {
      // we jumped from the end of list to the middle.
      int32_t back_offset = index - list->prev_ins_pos - NOPS_BEFORE_EACH_INST;
      // LOG(ERROR) << "HandleInstruction goback " << iter->second
      //     << " " << map_and_list->prev_ins_pos << " " << back_offset;
      uint32_t idx;
      // Add nops before each instruction, just for convenience of combine branches
      for (idx = 0; idx < NOPS_BEFORE_EACH_INST; ++idx) {
        list->code_list->push_back(0);
      }
      list->code_list->push_back(DUMP_GOTO_INSTRUCTION);
      list->code_list->push_back(static_cast<uint16_t>(back_offset & 0xffff));
      list->code_list->push_back(static_cast<uint16_t>((back_offset & 0xffff0000) >> 16));
    }
    list->prev_ins_pos = index;

    if (!IsSameInstruction(code, list->code_list, index, count)) {
      // different instructions in same pos, switch to a hack branch
      MapAndList* branch_list = new MapAndList(list, index);
      PushInstructionToList(branch_list, code, dex_pc, count, false);
      list = branch_list;
    }
  } else {
    // It is a new instruction in this list. Check whether we should end this branch.
    if (list->parent) {
      uint32_t index2 = list->parent->FindCodeInCodeMap(dex_pc);
      if (index2 != CODE_NO_INDEX) {
        if (IsSameInstruction(code, list->parent->code_list, index2, count)) {
          // end this branch, go back to parent
          list->end_pos = index2;
          list->parent->prev_ins_pos = index2;
          list = list->parent;
          TIME_MEASURE_END
          return;
        }  // else is still different from parent, keep in this branch.
      }  // else this instruction could not be found in parent, just keep in this branch;
    }
    // just stay in this branch, and add the instruction to list.
    PushInstructionToList(list, code, dex_pc, count, is_move_result_ins);
  }

  TIME_MEASURE_END
}

static inline void HandleFillArrayData(MapAndList*& list, uint16_t ins_data, ShadowFrame& shadow_frame,
    const Instruction::ArrayDataPayload* payload) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  TIME_MEASURE_BEGIN
  uint32_t dex_pc = shadow_frame.GetDexPC();

  uint32_t index = list->FindCodeInCodeMap(dex_pc);
  if (index != CODE_NO_INDEX) {
    if (list->prev_ins_pos == list->code_list->size()) {
      // we jumped from the end of list to the middle.
      int32_t back_offset = index - list->prev_ins_pos - NOPS_BEFORE_EACH_INST;
      // LOG(ERROR) << "HandleFillArrayData goback " << iter->second
      //     << " " << map_and_list->prev_ins_pos << " " << back_offset;
      uint32_t idx;
      // Add nops before each instruction, just for convenience of combine branches
      for (idx = 0; idx < NOPS_BEFORE_EACH_INST; ++idx) {
        list->code_list->push_back(0);
      }
      list->code_list->push_back(DUMP_GOTO_INSTRUCTION);
      list->code_list->push_back(static_cast<uint16_t>(back_offset & 0xffff));
      list->code_list->push_back(static_cast<uint16_t>((back_offset & 0xffff0000) >> 16));
    }
    list->prev_ins_pos = index;

    if (!IsSameFillArrayDataInstruction(list, ins_data, dex_pc, index, payload)) {
      // different instructions in same pos, switch to a hack branch
      MapAndList* branch_list = new MapAndList(list, index);
      // As now we are in a new list, the offset should be the length of this instruction, to the next instruction.
      PushFillArrayDataInstructionToList(branch_list, ins_data, dex_pc, payload);
      list = branch_list;
    }
  } else {
    // It is a new instruction in this list. Check whether we should end this branch.
    if (list->parent) {
      uint32_t index2 = list->parent->FindCodeInCodeMap(dex_pc);
      if (index2 != CODE_NO_INDEX) {
        if (IsSameFillArrayDataInstruction(list->parent, ins_data, dex_pc, index2, payload)) {
          // end this branch, go back to parent status
          list->end_pos = index2;
          list->parent->prev_ins_pos = index2;
          list = list->parent;
          TIME_MEASURE_END
          return;
        }  // else is still different from parent, keep in this branch.
      }  // else this instruction could not be found in parent, just keep in this branch;
    }
    // just stay in this branch, and add the instruction to list.
    PushFillArrayDataInstructionToList(list, ins_data, dex_pc, payload);
  }
  TIME_MEASURE_END
}

static inline void HandleGoto(MapAndList*& list, ShadowFrame& shadow_frame,
    int32_t offset) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  TIME_MEASURE_BEGIN
  uint32_t dex_pc = shadow_frame.GetDexPC();

  uint32_t index = list->FindCodeInCodeMap(dex_pc);
  if (index != CODE_NO_INDEX) {
    if (list->prev_ins_pos == list->code_list->size()) {
      // we jumped from the end of list to the middle.
      int32_t back_offset = index - list->prev_ins_pos - NOPS_BEFORE_EACH_INST;
      // LOG(ERROR) << "HandleGoto goback " << iter->second
      //     << " " << map_and_list->prev_ins_pos << " " << back_offset;
      uint32_t idx;
      // Add nops before each instruction, just for convenience of combine branches
      for (idx = 0; idx < NOPS_BEFORE_EACH_INST; ++idx) {
        list->code_list->push_back(0);
      }
      list->code_list->push_back(DUMP_GOTO_INSTRUCTION);
      list->code_list->push_back(static_cast<uint16_t>(back_offset & 0xffff));
      list->code_list->push_back(static_cast<uint16_t>((back_offset & 0xffff0000) >> 16));
    }

    int32_t offset_in_current = GetOffsetForPos(list, dex_pc, offset);
    if (!IsSameGotoInstruction(list, index, offset_in_current - index)) {
      // different instructions in same pos, switch to a hack branch
      MapAndList* branch_list = new MapAndList(list, index);
      // As now we are in a new list, the offset should be the length of this instruction, to the next instruction.
      PushGotoInstructionToList(branch_list, dex_pc, 3 + NOPS_BEFORE_EACH_INST);
      list = branch_list;
    } else {
      list->prev_ins_pos = offset_in_current;
    }
  } else {
    // It is a new instruction in this list. Check whether we should end this branch.
    if (list->parent) {
      uint32_t index2 = list->parent->FindCodeInCodeMap(dex_pc);
      if (index2 != CODE_NO_INDEX) {
        int32_t offset_in_parent = GetOffsetForPos(list->parent, dex_pc, offset);
        if (IsSameGotoInstruction(list->parent, index2, offset_in_parent - index2)) {
          // end this branch, go back to parent status
          list->end_pos = index2;
          list->parent->prev_ins_pos = offset_in_parent;
          list = list->parent;
          TIME_MEASURE_END
          return;
        }  // else is still different from parent, keep in this branch.
      }  // else this instruction could not be found in parent, just keep in this branch;
    }
    // just stay in this branch, and add the instruction to list.
    uint32_t index3 = list->FindCodeInCodeMap(dex_pc + offset);
    if (index3 != CODE_NO_INDEX) {
      PushGotoInstructionToList(list, dex_pc, index3 - list->code_list->size() - NOPS_BEFORE_EACH_INST);
    } else {
      // 3 is the length of this goto instruction
      PushGotoInstructionToList(list, dex_pc, 3 + NOPS_BEFORE_EACH_INST);
    }
  }
  TIME_MEASURE_END
}

static inline void HandleSwitch(MapAndList*& list, uint16_t ins_data,
    ShadowFrame& shadow_frame, int32_t offset, int32_t key) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  TIME_MEASURE_BEGIN
  uint32_t dex_pc = shadow_frame.GetDexPC();

  uint16_t instruction = DUMP_SWITCH_INSTRUCTION | (ins_data & 0xff00);
  bool is_default = offset == 3;

  uint32_t index = list->FindCodeInCodeMap(dex_pc);
  if (index != CODE_NO_INDEX) {
    if (list->prev_ins_pos == list->code_list->size()) {
      // we jumped from the end of list to the middle.
      int32_t back_offset = index - list->prev_ins_pos - NOPS_BEFORE_EACH_INST;
      // LOG(ERROR) << "HandleSwitch goback " << iter->second
      //     << " " << map_and_list->prev_ins_pos << " " << back_offset;
      uint32_t idx;
      // Add nops before each instruction, just for convenience of combine branches
      for (idx = 0; idx < NOPS_BEFORE_EACH_INST; ++idx) {
        list->code_list->push_back(0);
      }
      list->code_list->push_back(DUMP_GOTO_INSTRUCTION);
      list->code_list->push_back(static_cast<uint16_t>(back_offset & 0xffff));
      list->code_list->push_back(static_cast<uint16_t>((back_offset & 0xffff0000) >> 16));
    }

    int32_t offset_in_current = GetOffsetForPos(list, dex_pc, offset);
    if (!IsSameSwitchInstruction(list, instruction, dex_pc, index, offset_in_current - index, key, is_default)) {
      // different instructions in same pos, switch to a hack branch
      MapAndList* branch_list = new MapAndList(list, index);
      PushSwitchInstructionToList(branch_list, instruction, dex_pc, 6 + NOPS_BEFORE_EACH_INST, key, is_default);
      list = branch_list;
    } else {
      list->prev_ins_pos = index + offset_in_current;
    }
  } else {
    if (list->parent) {
      // We are in hack branch, check whether we should end this branch;
      uint32_t index2 = list->parent->FindCodeInCodeMap(dex_pc);
      if (index2 != CODE_NO_INDEX) {
        int32_t offset_in_parent = GetOffsetForPos(list->parent, dex_pc, offset);
        if (IsSameSwitchInstruction(list->parent, instruction, dex_pc, index2,
            offset_in_parent - index2, key, is_default)) {
          // end this branch, go back to parent status
          list->end_pos = index2;
          list->parent->prev_ins_pos = index2 + offset_in_parent;
          list = list->parent;
          TIME_MEASURE_END
          return;
        }  // else is still different from parent, keep in this branch.
      }  // else this instruction could not be found in parent, just keep in this branch;
    }
    // just stay in this branch, and add the instruction to list.
    uint32_t index3 = list->FindCodeInCodeMap(dex_pc + offset);
    if (index3 != CODE_NO_INDEX) {
      PushSwitchInstructionToList(list, instruction, dex_pc,
          index3 - list->code_list->size() - NOPS_BEFORE_EACH_INST, key, is_default);
    } else {
      // 6 is the length of this switch instruction
      PushSwitchInstructionToList(list, instruction, dex_pc, 6 + NOPS_BEFORE_EACH_INST, key, is_default);
    }
  }
  TIME_MEASURE_END
}

static inline void HandleIf(MapAndList*& list, uint16_t ins_data,
    ShadowFrame& shadow_frame, int32_t offset) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  TIME_MEASURE_BEGIN
  uint32_t dex_pc = shadow_frame.GetDexPC();

  bool is_else = offset == 2;
  uint32_t index = list->FindCodeInCodeMap(dex_pc);
  if (index != CODE_NO_INDEX) {
    if (list->prev_ins_pos == list->code_list->size()) {
      // we jumped from the end of list to the middle.
      int32_t back_offset = index - list->prev_ins_pos - NOPS_BEFORE_EACH_INST;
      // LOG(ERROR) << "HandleIf goback " << iter->second
      //     << " " << map_and_list->prev_ins_pos << " " << back_offset;
      uint32_t idx;
      // Add nops before each instruction, just for convenience of combine branches
      for (idx = 0; idx < NOPS_BEFORE_EACH_INST; ++idx) {
        list->code_list->push_back(0);
      }
      list->code_list->push_back(DUMP_GOTO_INSTRUCTION);
      list->code_list->push_back(static_cast<uint16_t>(back_offset & 0xffff));
      list->code_list->push_back(static_cast<uint16_t>((back_offset & 0xffff0000) >> 16));
    }

    int32_t offset_in_current = GetOffsetForPos(list, dex_pc, offset);
    if (!IsSameIfInstruction(list, ins_data, index, offset_in_current - index, is_else)) {
      // different instructions in same pos, switch to a hack branch
      MapAndList* branch_list = new MapAndList(list, index);
      PushIfInstructionToList(branch_list, ins_data, dex_pc, 8 + NOPS_BEFORE_EACH_INST, is_else);
      list = branch_list;
    } else {
      list->prev_ins_pos = index + offset_in_current;
    }
  } else {
    if (list->parent) {
      int32_t offset_in_parent = GetOffsetForPos(list->parent, dex_pc, offset);
      // We are in hack branch, check whether we should end this branch;
      uint32_t index2 = list->parent->FindCodeInCodeMap(dex_pc);
      if (index2 != CODE_NO_INDEX) {
        if (IsSameIfInstruction(list->parent, ins_data, index2, offset_in_parent - index2, is_else)) {
          // end this branch, go back to parent status
          list->end_pos = index2;
          list->parent->prev_ins_pos = index2 + offset_in_parent;
          list = list->parent;
          TIME_MEASURE_END
          return;
        }  // else is still different from parent, keep in this branch.
      }  // else this instruction could not be found in parent, just keep in this branch;
    }
    // just stay in this branch, and add the instruction to list.
    uint32_t index3 = list->FindCodeInCodeMap(dex_pc + offset);
    if (index3 != CODE_NO_INDEX) {
      PushIfInstructionToList(list, ins_data, dex_pc, index3 - list->code_list->size() - NOPS_BEFORE_EACH_INST, is_else);
    } else {
      // 8 is the length of this if instruction
      PushIfInstructionToList(list, ins_data, dex_pc, 8 + NOPS_BEFORE_EACH_INST, is_else);
    }
  }
  TIME_MEASURE_END
}

static inline uint16_t* HandleString(uint16_t inst_data, uint32_t string_idx, ShadowFrame& shadow_frame) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  // LOG(ERROR) << "HandleString begin";
  const DexFile* dex_file = shadow_frame.GetMethod()->GetDexFile();

  uint32_t index = Dumper::Instance()->DumpStringFromDex(*dex_file, string_idx);

  // LOG(ERROR) << "HandleString target string " << string << ", " << string_idx << ", " << index;
  // DexFile::CodeItem* item = reinterpret_cast<DexFile::CodeItem*>(dumpedCodeItem);
  // LOG(ERROR) << "HandleString src " << inst_data << " " << string_idx;
  uint16_t* new_inst = new uint16_t[3];
  new_inst[0] = 0x1B | (inst_data & 0xff00);
  new_inst[1] = index & 0xffff;
  new_inst[2] = static_cast<uint16_t>((index & 0xffff0000) >> 16);
  // LOG(ERROR) << "HandleString dst " << new_inst[0] << " " << new_inst[1] << " " << new_inst[2];
  // LOG(ERROR) << "HandleString end";
  return new_inst;
}

static inline uint16_t* HandleType(const uint16_t* codes, uint32_t type_idx, ShadowFrame& shadow_frame, uint32_t count) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  // LOG(ERROR) << "HandleType begin";
  const DexFile* dex_file = shadow_frame.GetMethod()->GetDexFile();

  uint16_t index = Dumper::Instance()->DumpTypeFromDex(*dex_file, type_idx);

  // LOG(ERROR) << "HandleType target string " << ds.string_ << ", " << type_idx << ", " << index;
  // DexFile::CodeItem* item = reinterpret_cast<DexFile::CodeItem*>(dumpedCodeItem);
  // LOG(ERROR) << "HandleType src " << codes[0] << " " << codes[1];
  uint16_t* new_inst = new uint16_t[count];
  memcpy(new_inst, codes, count * 2);
  new_inst[1] = index;
  // LOG(ERROR) << "HandleType dst " << new_inst[0] << " " << new_inst[1];
  // LOG(ERROR) << "HandleType end";
  return new_inst;
}

#ifdef ELIMINATE_REFLECTION
template<InvokeType type, bool is_range, bool do_access_check>
uint16_t EliminateReflection(DumpString* declaring_clz, mirror::Object* arg0, mirror::Object* receiver, mirror::ObjectArray<mirror::Object>* target_param_array,
    ArtMethod* sf_method) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ArtMethod* target = Dumper::Instance()->GetTargetMethod(arg0);
  // target = receiver->GetClass()->FindVirtualMethodForVirtualOrInterface(target, sizeof(void*));
  // LOG(ERROR)<< "got params array " << target_param_array;
  uint8_t param_size = target_param_array->GetLength();
  // LOG(ERROR)<< "got param_size " << (param_size + 48);
  DumpMethodInfo* target_method_info = new DumpMethodInfo;
  Dumper::Instance()->GetMethodInfo(target_method_info, target);
  // LOG(ERROR)<< "got target_method_info ";
  uint32_t index = Dumper::Instance()->DumpDexMethod(target_method_info, UNKNOWN, nullptr);
  // LOG(ERROR)<< "HandleInvoke InvokeType " << type << " target method " << target->GetName() << " " << index;
  const char* target_declaring_clz_desc = target->GetDeclaringClassDescriptor();
  DumpMethodInfo* caller_method_info = new DumpMethodInfo;
  Dumper::Instance()->GetMethodInfo(caller_method_info, sf_method);
  // create a new method
  DumpMethodInfo* new_method_info = new DumpMethodInfo;
  new_method_info->location_ = caller_method_info->location_;
  // LOG(ERROR)<< "new_method_info location_ " << new_method_info->location_;
  if (declaring_clz) {
    new_method_info->class_name_ = *declaring_clz;
  } else {
    new_method_info->class_name_ = caller_method_info->class_name_;
  }
  // LOG(ERROR)<< "new_method_info class_name_ " << new_method_info->class_name_.string_;
  new_method_info->access_flag_ = 0x8 | 0x1;
  // LOG(ERROR)<< "new_method_info access_flag_ " << new_method_info->access_flag_;
  new_method_info->tries_size_ = 0;
  char* new_method_name = new char[100];
  std::hash < std::string > hash;
  sprintf(new_method_name, "revealer_%zu_%s", hash(target_declaring_clz_desc), target_method_info->name_.string_.c_str());
  DumpString* ds = new DumpString;
  ds->string_ = new_method_name;
  ds->string_length_ = ds->string_.length();
  new_method_info->name_ = *ds;
  // LOG(ERROR)<< "new_method_info name_ " << new_method_info->name_.string_;
  ds = new DumpString;
  ds->string_ = target_method_info->shorty_.string_.substr(0, 1) + "LL";
  ds->string_length_ = ds->string_.length();
  new_method_info->shorty_ = *ds;
  // LOG(ERROR)<< "new_method_info shorty_ " << new_method_info->shorty_.string_;
  // class object and array object
  new_method_info->outs_size_ = target->IsStatic() ? param_size : param_size + 1;
  // LOG(ERROR)<< "new_method_info outs_size_ " << new_method_info->outs_size_;
  uint16_t return_size = 0;
  ds = new DumpString;
  ds->string_ = "Ljava/lang/Object;";
  if (target_method_info->shorty_.string_[0] == 'J' || target_method_info->shorty_.string_[0] == 'D') {
    return_size = 2;
  } else if (target_method_info->shorty_.string_[0] == 'V') {
    return_size = 0;
    ds->string_ = "V";
  } else {
    return_size = 1;
  }

  ds->string_length_ = ds->string_.length();
  new_method_info->return_name_ = *ds;
  // LOG(ERROR)<< "new_method_info return_name_ " << new_method_info->return_name_.string_ << "," << new_method_info->return_name_.string_length_;
  new_method_info->params_types_ = new std::vector<DumpString>;
  ds = new DumpString;
  ds->string_ = target_declaring_clz_desc;
  ds->string_length_ = ds->string_.length();
  new_method_info->params_types_->push_back(*ds);
  ds = new DumpString;
  ds->string_ = "[Ljava/lang/Object;";
  ds->string_length_ = ds->string_.length();
  new_method_info->params_types_->push_back(*ds);
  new_method_info->ins_size_ = 2;
  new_method_info->registers_size_ = new_method_info->ins_size_ + param_size + 1;
  // LOG(ERROR)<< "new_method_info registers_size_ " << new_method_info->registers_size_;
  // 14 is additional data added in CombineCode
  uint32_t code_size = 3 + 4 * (param_size) + 5 + 14;
  size_t size_code_units = 2 * 4 + 4 * 2 + 2 * code_size;
  // LOG(ERROR)<< "new_method_info size_code_units " << size_code_units;
  DumpCodeItem* new_code_item = reinterpret_cast<DumpCodeItem*>(malloc(size_code_units));
  new_code_item->registers_size_ = new_method_info->registers_size_;
  // LOG(ERROR)<< "new_code_item registers_size_ " << new_code_item->registers_size_;
  new_code_item->ins_size_ = new_method_info->ins_size_;
  // LOG(ERROR)<< "new_code_item ins_size_ " << new_code_item->ins_size_;
  new_code_item->outs_size_ = new_method_info->outs_size_;
  // LOG(ERROR)<< "new_code_item outs_size_ " << new_code_item->outs_size_;
  new_code_item->tries_size_ = new_method_info->tries_size_;
  // LOG(ERROR)<< "new_code_item tries_size_ " << new_code_item->tries_size_;
  new_code_item->debug_info_off_ = 0;
  new_code_item->insns_size_in_code_units_ = code_size;
  // LOG(ERROR)<< "new_code_item insns_size_in_code_units_ " << new_code_item->insns_size_in_code_units_;
  // uint32_t new_idx = Dumper::Instance()->addCommonInstructions(new_code_item, true, code_size);
  uint32_t new_idx = 0;
  uint16_t ins = 0;
  // start pos
  memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
  // LOG(ERROR)<< "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
  memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
  // LOG(ERROR)<< "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
  // end pos
  memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
  // LOG(ERROR)<< "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
  memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
  // LOG(ERROR)<< "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
  // code size
  ins = static_cast<uint16_t>((code_size - 14) & 0xffff);
  memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
  // LOG(ERROR)<< "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
  ins = static_cast<uint16_t>(((code_size - 14) & 0xffff0000) >> 16);
  memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
  // LOG(ERROR)<< "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
  // real code
  // move p0 to v0
  ins = 0x09;
  memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
  // LOG(ERROR)<< "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
  ins = static_cast<uint16_t>(0);
  memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
  // LOG(ERROR)<< "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
  // register param_size + 1(p0) is this of the target class,
  ins = static_cast<uint16_t>(param_size + 1);
  memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
  // LOG(ERROR)<< "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
  uint8_t pidx = 0;
  for (pidx = 0; pidx < param_size; ++pidx) {
    // register param_size is the array idx
    ins = 0x13 | (param_size << 8);
    memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
    // LOG(ERROR)<< "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
    ins = static_cast<uint16_t>(pidx);
    memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
    // LOG(ERROR)<< "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
    ins = 0x46 | ((pidx + 1) << 8);
    memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
    // LOG(ERROR)<< "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
    // array register param_size + 2
    ins = (param_size + 2) | (param_size << 8);
    memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
    // LOG(ERROR)<< "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
  }
  // LOG(ERROR)<< "param_size " << param_size;

  bool target_resolve = false;
  if (strncmp(target->GetName(), "invoke", strlen("invoke")) == 0) {
    /* for (pidx = param_size - 1; pidx >= 0; --pidx) {
      LOG(ERROR)<< "traverse index " << pidx;
      mirror::Object* temp_obj = target_param_array->Get(pidx);
      mirror::Class* temp_clz = temp_obj->GetClass();
      if (temp_clz) {
        LOG(ERROR)<< "temp_clz " << temp_clz;
        mirror::String* temp_name = temp_clz->GetName();
        if (temp_name) {
          LOG(ERROR)<< "temp_name " << temp_name;
          LOG(ERROR)<< "temp_name utf8 " << temp_name->ToModifiedUtf8();
        } else {
          LOG(ERROR)<< "empty name";
        }
      } else {
        LOG(ERROR)<< "empty objects";
      }
    } */
    target_resolve = true;
    index = EliminateReflection<type, is_range, do_access_check>(&new_method_info->class_name_, receiver, target_param_array->Get(0),
        target_param_array->Get(1)->AsObjectArray<mirror::Object>(), target);
  }

  if (target->IsStatic() || target_resolve) {
    ins = 0x77 | (param_size << 8);
  } else {
    ins = 0x74 | ((param_size + 1) << 8);
  }
  memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
  // LOG(ERROR)<< "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];

  ins = static_cast<uint16_t>(index);
  memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);


  // LOG(ERROR)<< "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
  if (target->IsStatic()) {
    ins = 1;
  } else {
    ins = 0;
  }
  memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
  // LOG(ERROR)<< "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
  if (return_size == 2) {
    ins = 0x0b | ((param_size) << 8);
    memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
    // LOG(ERROR)<< "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
    ins = 0x10 | ((param_size) << 8);
    memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
    // LOG(ERROR)<< "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
  } else if (return_size == 1) {
    ins = 0x0c | ((param_size) << 8);
    memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
    // LOG(ERROR) << "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
    ins = 0x11 | ((param_size) << 8);
    memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
    // LOG(ERROR) << "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
  } else {
    ins = 0;
    memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
    // LOG(ERROR) << "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
    ins = 0x0e;
    memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
    // LOG(ERROR) << "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
  }

  // new_idx = Dumper::Instance()->addCommonInstructions(new_code_item, false, code_size);
  ins = 0;
  // code map size
  memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
  // LOG(ERROR)<< "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
  memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
  // LOG(ERROR)<< "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
  // switch table size
  memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
  // LOG(ERROR)<< "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
  memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
  // LOG(ERROR)<< "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
  // array data size
  memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
  // LOG(ERROR)<< "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
  memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
  // LOG(ERROR)<< "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
  // child size
  memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
  // LOG(ERROR)<< "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
  memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
  // LOG(ERROR)<< "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
  uint32_t new_method_idx = Dumper::Instance()->DumpDexMethod(new_method_info, VIRTUAL, new_code_item);
  free(new_code_item);
  delete new_method_info;
  delete target_method_info;

  return new_method_idx;
}

#endif

template<InvokeType type, bool is_range, bool do_access_check>
static inline uint16_t* HandleInvoke(const uint16_t* codes, const ShadowFrame& shadow_frame,
    __attribute__((unused))ArtMethod* method, uint32_t count) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  // LOG(ERROR) << "HandleInvoke begin" << " " << self;
  // const uint32_t method_idx = (is_range) ? inst->VRegB_3rc() : inst->VRegB_35c();
  // const uint32_t vregC = (is_range) ? inst->VRegC_3rc() : inst->VRegC_35c();
  // mirror::Object* receiver = (type == kStatic) ? nullptr : shadow_frame.GetVRegReference(vregC);
  // ArtMethod* sf_method = shadow_frame.GetMethod();
  // ArtMethod* const method = FindMethodFromCode<type, do_access_check>(method_idx, &receiver, &sf_method, self);

//  DumpMethodInfo* target_method_info_in_dex = new DumpMethodInfo;
//  Dumper::Instance()->GetMethodInfo(target_method_info_in_dex, method);

  uint16_t* new_inst = NULL;
  uint16_t invoke_start_index = 0;
  bool assigned = false;

  /* const char* class_desc = method->GetDeclaringClassDescriptor();
  Class* declaring_class = method->GetDeclaringClass();
  const char* name = method->GetName();

  if (strncmp(name, "loadClass", strlen(name)) == 0) {
   mirror::Class* java_lang_ClassLoader =
   Runtime::Current()->GetClassLinker()->FindSystemClass(self, "Ljava/lang/ClassLoader;");
   if (java_lang_ClassLoader->IsAssignableFrom(declaring_class)) {
   uint16_t regList = inst->Fetch16(2);
   mirror::String* class_name_param =
   shadow_frame.GetVRegReference<kVerifyNone>((regList >>= 4) & 0x0f)->AsString();
   uint32_t name_index = Dumper::Instance()->GetStringIndex(
   method->GetDexFile()->GetLocation(), class_name_param->ToModifiedUtf8());

   uint16_t new_size = count + 1 + 3;
   new_inst = new uint16_t[new_size];
   new_inst[0] = new_size - 1;
   new_inst[1] = 0x1b | ((codes[2] & 0xf0) << 4);
   new_inst[2] = static_cast<uint16_t>(name_index & 0xffff);
   new_inst[3] = static_cast<uint16_t>((name_index & 0xffff0000) >> 16);

   invoke_start_index = 4;
   assigned = true;
   }
   } else if (strncmp(name, "forName", strlen(name)) == 0
   || strncmp(name, "getDeclaredMethod", strlen(name)) == 0
   || strncmp(name, "getMethod", strlen(name)) == 0
   || strncmp(name, "getDeclaredField", strlen(name)) == 0
   || strncmp(name, "getField", strlen(name)) == 0) {
   mirror::Class* java_lang_ClassLoader =
   Runtime::Current()->GetClassLinker()->FindSystemClass(self, "Ljava/lang/Class;");
   if (java_lang_ClassLoader->IsAssignableFrom(declaring_class)) {
   uint16_t regList = inst->Fetch16(2);
   mirror::String* class_name_param =
   shadow_frame.GetVRegReference<kVerifyNone>((regList >>= 4) & 0x0f)->AsString();
   uint32_t name_index = Dumper::Instance()->GetStringIndex(
   method->GetDexFile()->GetLocation(), class_name_param->ToModifiedUtf8());

   uint16_t new_size = count + 1 + 3;
   new_inst = new uint16_t[new_size];
   new_inst[0] = new_size - 1;
   new_inst[1] = 0x1b | ((codes[2] & 0xf0) << 4);
   new_inst[2] = static_cast<uint16_t>(name_index & 0xffff);
   new_inst[3] = static_cast<uint16_t>((name_index & 0xffff0000) >> 16);

   invoke_start_index = 4;
   assigned = true;
   }
   } */
  /*
  if (strncmp(class_desc, "Ljava/lang/reflect/Method;", strlen(class_desc)) == 0
      && strncmp(name, "invoke", strlen(name)) == 0) {
    LOG(ERROR) << "Find reflective invoking";
    uint16_t regList = inst->Fetch16(2);
    //  arg0 this of the method
    mirror::Object* o = shadow_frame.GetVRegReference<kVerifyNone>(regList & 0x0f);
    // arg1 the target
    uint4_t reg_for_target_this = (regList >> 4) & 0x0f;
    LOG(ERROR)<< "got target this " << (reg_for_target_this + 48);
    //  arg2 the parameter array
    uint4_t params = (regList >> 8) & 0x0f;
    LOG(ERROR)<< "got target params " << (params + 48);
    mirror::ObjectArray<mirror::Object>* target_param_array = shadow_frame.GetVRegReference<kVerifyNone>(params)->AsObjectArray<mirror::Object>();
    uint16_t new_method_idx = EliminateReflection<type, is_range, do_access_check>(nullptr, o, shadow_frame.GetVRegReference<kVerifyNone>(reg_for_target_this),
        target_param_array, sf_method);
    uint16_t new_size = count + 1;
    new_inst = new uint16_t[new_size];
    new_inst[0] = count;
    invoke_start_index = 1;
    new_inst[1] = 0x2071;
    new_inst[2] = static_cast<uint16_t>(new_method_idx);
    new_inst[3] = ((regList >> 4) & 0x0f) | (((regList >> 8) & 0x0f) << 4);
    return new_inst;
    LOG(ERROR) << "HandleInvoke InvokeType " << type << " dst " << new_inst[1] << " " << new_inst[2] << " " << new_inst[3];
    LOG(ERROR) << "HandleInvoke end";

    return new_inst;
  } else if (strncmp(name, "newInstance", strlen(name)) == 0 || strncmp(name, "newInstanceImpl", strlen(name)) == 0) {
    mirror::Class* java_lang_Class =
    Runtime::Current()->GetClassLinker()->FindSystemClass(self, "Ljava/lang/Class;");
    if (java_lang_Class->IsAssignableFrom(declaring_class)) {
      uint16_t regList = inst->Fetch16(2);
      mirror::Class* clz = shadow_frame.GetVRegReference<kVerifyNone>(regList & 0x0f)->AsClass();
      std::string clz_store;
      const char* clz_desc = clz->GetDescriptor(&clz_store);

      DumpMethodInfo* caller_method_info = new DumpMethodInfo;
      Dumper::Instance()->GetMethodInfo(caller_method_info, sf_method);

      // create a new method
      DumpMethodInfo* new_method_info = new DumpMethodInfo;
      new_method_info->location_ = caller_method_info->location_;
      LOG(ERROR) << "new_method_info location_ " << new_method_info->location_;
      new_method_info->class_name_ = caller_method_info->class_name_;
      LOG(ERROR) << "new_method_info class_name_ " << new_method_info->class_name_.string_;
      new_method_info->access_flag_ = 0x8 | 0x1;
      LOG(ERROR) << "new_method_info access_flag_ " << new_method_info->access_flag_;
      new_method_info->outs_size_ = 1;
      LOG(ERROR) << "new_method_info outs_size_ " << new_method_info->outs_size_;
      new_method_info->tries_size_ = 0;
      DumpString* ds = new DumpString;
      ds->string_ = clz_desc;
      ds->string_length_ = ds->string_.length();
      new_method_info->return_name_ = *ds;
      LOG(ERROR) << "new_method_info return_name_ " << new_method_info->return_name_.string_;
      char* new_method_name = new char[100];
      std::hash<std::string> hash;
      sprintf(new_method_name, "revealer_%zu_%s", hash(clz_desc), name);
      ds = new DumpString;
      ds->string_ = new_method_name;
      ds->string_length_ = ds->string_.length();
      new_method_info->name_ = *ds;
      LOG(ERROR) << "new_method_info name_ " << new_method_info->name_.string_;
      ds = new DumpString;
      ds->string_ = "L";
      ds->string_length_ = ds->string_.length();
      new_method_info->shorty_ = *ds;
      LOG(ERROR) << "new_method_info shorty_ " << new_method_info->shorty_.string_;
      new_method_info->params_types_ = new std::vector<DumpString>;
      new_method_info->ins_size_= 0;
      new_method_info->registers_size_= 1;

      // 14 is additional data added in CombineCode
      uint32_t code_size = 6 + 14;
      size_t size_code_units = 2 * 4 + 4 * 2 + 2 * code_size;
      LOG(ERROR) << "new_method_info size_code_units " << size_code_units;
      DumpCodeItem* new_code_item = reinterpret_cast<DumpCodeItem*>(malloc(size_code_units));
      new_code_item->registers_size_= new_method_info->registers_size_;
      LOG(ERROR) << "new_code_item registers_size_ " << new_code_item->registers_size_;
      new_code_item->ins_size_= new_method_info->ins_size_;
      LOG(ERROR) << "new_code_item ins_size_ " << new_code_item->ins_size_;
      new_code_item->outs_size_ = new_method_info->outs_size_;
      LOG(ERROR) << "new_code_item outs_size_ " << new_code_item->outs_size_;
      new_code_item->tries_size_ = new_method_info->tries_size_;
      LOG(ERROR) << "new_code_item tries_size_ " << new_code_item->tries_size_;
      new_code_item->debug_info_off_ = 0;
      new_code_item->insns_size_in_code_units_ = code_size;

      // uint32_t new_idx = Dumper::Instance()->addCommonInstructions(new_code_item, true, code_size);
      uint32_t new_idx = 0;
      uint16_t ins = 0;
      // start pos
      memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
      LOG(ERROR) << "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
      memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
      LOG(ERROR) << "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
      // end pos
      memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
      LOG(ERROR) << "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
      memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
      LOG(ERROR) << "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
      // code size
      ins = static_cast<uint16_t>((code_size - 14) & 0xffff);
      memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
      LOG(ERROR) << "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
      ins = static_cast<uint16_t>(((code_size - 14) & 0xffff0000) >> 16);
      memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
      LOG(ERROR) << "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];

      // real code
      // new instance
      ins = 0x22;
      memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
      LOG(ERROR) << "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
      ds = new DumpString;
      ds->string_ = clz_desc;
      ds->string_length_ = ds->string_.length();
      uint16_t type_idx = Dumper::Instance()->GetTypeIndex(caller_method_info->location_, *ds);
      memcpy(&(new_code_item->insns_[new_idx++]), &type_idx, 2);
      LOG(ERROR) << "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
      // init
      ins = 0x1070;
      memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
      LOG(ERROR) << "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
      ArtMethod* init_method = clz->FindDirectMethod("<init>", "()V", sizeof(clz));
      LOG(ERROR) << "get init_method " << init_method;
      uint32_t init_method_idx = Dumper::Instance()->DumpDexMethod(caller_method_info->location_, DIRECT, init_method, nullptr);
      LOG(ERROR) << "get init_method_idx " << init_method_idx;
      ins = static_cast<uint16_t>(init_method_idx);
      memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
      LOG(ERROR) << "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
      ins = 0;
      memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
      LOG(ERROR) << "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
      // return
      ins = 0x11;
      memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
      LOG(ERROR) << "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];

      // new_idx = Dumper::Instance()->addCommonInstructions(new_code_item, false, code_size);

      ins = 0;
      // code map size
      memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
      LOG(ERROR) << "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
      memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
      LOG(ERROR) << "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
      // switch table size
      memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
      LOG(ERROR) << "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
      memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
      LOG(ERROR) << "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
      // array data size
      memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
      LOG(ERROR) << "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
      memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
      LOG(ERROR) << "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
      // child size
      memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
      LOG(ERROR) << "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];
      memcpy(&(new_code_item->insns_[new_idx++]), &ins, 2);
      LOG(ERROR) << "insns_ " << new_idx - 1 << " " << new_code_item->insns_[new_idx - 1];

      uint32_t new_method_idx = Dumper::Instance()->DumpDexMethod(new_method_info, VIRTUAL, new_code_item);

      free(new_code_item);
      delete new_method_info;
      delete caller_method_info;

      uint16_t new_size = count + 1;
      new_inst = new uint16_t[new_size];
      new_inst[0] = count;
      invoke_start_index = 1;

      new_inst[1] = 0x0071;
      new_inst[2] = static_cast<uint16_t>(new_method_idx);
      new_inst[3] = 0;

      LOG(ERROR) << "HandleInvoke InvokeType " << type << " dst " << new_inst[1] << " " << new_inst[2] << " " << new_inst[3];
      LOG(ERROR) << "HandleInvoke end";

      return new_inst;
    }
  } */

  if (!assigned) {
    uint16_t new_size = count + 1;
    new_inst = new uint16_t[new_size];
    new_inst[0] = count;
    invoke_start_index = 1;
  }

  memcpy(&(new_inst[invoke_start_index]), codes, count * 2);

  // uint32_t index = Dumper::Instance()->DumpDexMethod(sf_method->GetDexFile()->GetLocation(), (type == kStatic || type == kDirect) ? DIRECT : VIRTUAL, method, nullptr);
//  mirror::Class* declaring_class = method->GetDeclaringClass();
//  Dumper::Instance()->DumpClassFromDex(declaring_class->GetDexFile(), *declaring_class->GetClassDef());
//  uint32_t index = Dumper::Instance()->DumpEncodedMethodFromDex(*method->GetDexFile(),
//          method->GetDexMethodIndex(), (type == kStatic || type == kDirect) ? DIRECT : VIRTUAL, method->GetAccessFlags());
//  uint32_t index = Dumper::Instance()->DumpMethodFromDex(*method->GetDexFile(), method->GetDexMethodIndex());  mirror::Class* declaring_class = field->GetDeclaringClass();

//  mirror::Class* declaring_class = method->GetDeclaringClass();
//  const DexFile& dex_file = declaring_class->GetDexFile();
  uint32_t index;
//  if (dex_file.GetLocation().compare(0, 8, "/system/") != 0) {
//    Dumper::Instance()->DumpClassFromDex(dex_file, *declaring_class->GetClassDef());
//    index = Dumper::Instance()->DumpEncodedMethodFromDex(*method->GetDexFile(),
//            method->GetDexMethodIndex(), (type == kStatic || type == kDirect) ? DIRECT : VIRTUAL, method->GetAccessFlags());
//  } else {
    index = Dumper::Instance()->DumpMethodFromDex(*shadow_frame.GetMethod()->GetDexFile(), codes[1]);
//  }

  // LOG(ERROR) << "HandleInvoke InvokeType " << type << " target method " /* << method->GetName() << ", " */ << method_idx << ", " << index;
  // DexFile::CodeItem* item = reinterpret_cast<DexFile::CodeItem*>(dumpedCodeItem);
  // LOG(ERROR) << "HandleInvoke InvokeType " << type << " src " << codes[0] << " " << codes[1] << " " << codes[2];
  new_inst[invoke_start_index + 1] = static_cast<uint16_t>(index);
  // LOG(ERROR) << "HandleInvoke InvokeType " << type << " dst " << new_inst[0] << " " << new_inst[1] << " " << new_inst[2];
  // LOG(ERROR) << "HandleInvoke end";

  return new_inst;
}

template<bool is_range>
static inline uint16_t* HandleInvokeQuick(const uint16_t* codes, __attribute__((unused))const ShadowFrame& shadow_frame,
    ArtMethod* method, uint32_t count) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  // LOG(ERROR) << "HandleInvokeQuick begin";
  uint16_t* new_inst = NULL;
  uint16_t invoke_start_index = 0;
  bool assigned = false;

  if (!assigned) {
    uint16_t new_size = count + 1;
    new_inst = new uint16_t[new_size];
    new_inst[0] = count;
    invoke_start_index = 1;
  }

  memcpy(&(new_inst[invoke_start_index]), codes, count * 2);

  // Dumper::Instance()->DumpDexMethod(VIRTUAL, target_method, nullptr);
//  mirror::Class* declaring_class = method->GetDeclaringClass();
//  Dumper::Instance()->DumpClassFromDex(declaring_class->GetDexFile(), *declaring_class->GetClassDef());
//  uint32_t target_index = Dumper::Instance()->DumpEncodedMethodFromDex(*method->GetDexFile(),
//          method->GetDexMethodIndex(), VIRTUAL, method->GetAccessFlags());
//  uint32_t target_index = Dumper::Instance()->DumpMethodFromDex(*method->GetDexFile(), method->GetDexMethodIndex());

  mirror::Class* declaring_class = method->GetDeclaringClass();
  const DexFile& dex_file = declaring_class->GetDexFile();
  uint32_t target_index;
//  if (dex_file.GetLocation().compare(0, 8, "/system/") != 0) {
//    Dumper::Instance()->DumpClassFromDex(dex_file, *declaring_class->GetClassDef());
//    target_index = Dumper::Instance()->DumpEncodedMethodFromDex(*method->GetDexFile(),
//            method->GetDexMethodIndex(), VIRTUAL, method->GetAccessFlags());
//  } else {
    target_index = Dumper::Instance()->DumpMethodFromDex(dex_file, method->GetDexMethodIndex());
//  }

  // LOG(ERROR) << "HandleInvokeQuick target method " << target_method->GetName() << ", " << vtable_idx << ", " << target_index;
  // LOG(ERROR) << "HandleInvokeQuick src " << codes[0] << " " << codes[1];
  uint8_t opcode = is_range ? static_cast<uint8_t>(0x74) : static_cast<uint8_t>(0x6E);
  new_inst[invoke_start_index] = (codes[0] & 0xff00) | opcode;
  new_inst[invoke_start_index + 1] = static_cast<uint16_t>(target_index);
  // LOG(ERROR) << "HandleInvokeQuick dst " << new_inst[0] << " " << new_inst[1];
  // LOG(ERROR) << "HandleInvokeQuick end";
  return new_inst;
}

template<FindFieldType find_type, Primitive::Type field_type, bool do_access_check>
uint16_t* HandleFieldGet(const uint16_t* codes, const ShadowFrame& shadow_frame,
    uint32_t count, __attribute__((unused))ArtField* field, __attribute__((unused))bool is_static) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
//  mirror::Class* declaring_class = field->GetDeclaringClass();
//  const DexFile& dex_file = declaring_class->GetDexFile();
  uint32_t index;
//  if (dex_file.GetLocation().compare(0, 8, "/system/") != 0) {
//    Dumper::Instance()->DumpClassFromDex(dex_file, *declaring_class->GetClassDef());
//    index = Dumper::Instance()->DumpEncodedFieldFromDex(dex_file,
//            field->GetDexFieldIndex(), is_static ? STATIC : INSTANCE, field->GetAccessFlags());
//  } else {
    index = Dumper::Instance()->DumpFieldFromDex(*shadow_frame.GetMethod()->GetDexFile(), codes[1]);
//  }

  // LOG(ERROR) << "HandleFieldGet FindFieldType " << find_type << " target field " << f->GetName() << ", " << index;
  // LOG(ERROR) << "HandleFieldGet FindFieldType " << find_type << " src " << codes[0] << " " << codes[1];
  uint16_t* new_inst = new uint16_t[count];
  memcpy(new_inst, codes, count * 2);
  new_inst[1] = static_cast<uint16_t>(index);
  // LOG(ERROR) << "HandleFieldGet FindFieldType " << find_type << " dst " << new_inst[0] << " " << new_inst[1];
  // LOG(ERROR) << "HandleFieldGet end";
  return new_inst;
}

template<Primitive::Type field_type>
uint16_t* HandleFieldGetQuick(const uint16_t* codes, __attribute__((unused))const ShadowFrame& shadow_frame,
    ArtField* field, uint32_t count) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::Class* declaring_class = field->GetDeclaringClass();
  const DexFile& dex_file = declaring_class->GetDexFile();
  uint32_t index;
//  if (dex_file.GetLocation().compare(0, 8, "/system/") != 0) {
////    Dumper::Instance()->DumpClassFromDex(dex_file, *declaring_class->GetClassDef());
//    index = Dumper::Instance()->DumpEncodedFieldFromDex(dex_file,
//            field->GetDexFieldIndex(), INSTANCE, field->GetAccessFlags());
//  } else {
    index = Dumper::Instance()->DumpFieldFromDex(dex_file, field->GetDexFieldIndex());
//  }

  // LOG(ERROR) << "HandleFieldGetQuick FindFieldType " << field_type << " target field " << f->GetName() << ", " << index;
  // LOG(ERROR) << "HandleFieldGetQuick FindFieldType " << field_type << " src " << codes[0] << " " << codes[1];
  uint8_t src_opcode = codes[0] & 0xff;
  uint8_t target_opcode;
  if (src_opcode == 0xE3) {
    target_opcode = static_cast<uint8_t>(0x52);
  } else if (src_opcode == 0xE4) {
    target_opcode = static_cast<uint8_t>(0x53);
  } else {
    target_opcode = static_cast<uint8_t>(0x54);
  }
  uint16_t* new_inst = new uint16_t[count];
  memcpy(new_inst, codes, count * 2);
  new_inst[0] = (codes[0] & 0xff00) | target_opcode;
  new_inst[1] = static_cast<uint16_t>(index);
  // LOG(ERROR) << "HandleFieldGetQuick FindFieldType " << field_type << " dst " << new_inst[0] << " " << new_inst[1];
  // LOG(ERROR) << "HandleFieldGetQuick end";
  return new_inst;
}

template<FindFieldType find_type, Primitive::Type field_type, bool do_access_check>
uint16_t* HandleFieldPut(const uint16_t* codes, const ShadowFrame& shadow_frame,
    uint32_t count, __attribute__((unused))ArtField* field, __attribute__((unused))bool is_static) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
//  mirror::Class* declaring_class = field->GetDeclaringClass();
//  const DexFile& dex_file = declaring_class->GetDexFile();
  uint32_t index;
//  if (dex_file.GetLocation().compare(0, 8, "/system/") != 0) {
////    Dumper::Instance()->DumpClassFromDex(dex_file, *declaring_class->GetClassDef());
//    index = Dumper::Instance()->DumpEncodedFieldFromDex(dex_file,
//            field->GetDexFieldIndex(), is_static ? STATIC : INSTANCE, field->GetAccessFlags());
//  } else {
    index = Dumper::Instance()->DumpFieldFromDex(*shadow_frame.GetMethod()->GetDexFile(), codes[1]);
//  }

  // LOG(ERROR) << "HandleFieldPut FindFieldType " << find_type << " target field " << f->GetName() << ", " << index;
  // LOG(ERROR) << "HandleFieldPut FindFieldType " << find_type << " src " << codes[0] << " " << codes[1];
  uint16_t* new_inst = new uint16_t[count];
  memcpy(new_inst, codes, count * 2);
  new_inst[1] = static_cast<uint16_t>(index);
  // LOG(ERROR) << "HandleFieldPut FindFieldType " << find_type << " dst " << new_inst[0] << " " << new_inst[1];
  // LOG(ERROR) << "HandleFieldPut end";
  return new_inst;
}

template<Primitive::Type field_type>
uint16_t* HandleFieldPutQuick(const uint16_t* codes, __attribute__((unused))const ShadowFrame& shadow_frame,
    ArtField* field, uint32_t count) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::Class* declaring_class = field->GetDeclaringClass();
  const DexFile& dex_file = declaring_class->GetDexFile();
  uint32_t index;
//  if (dex_file.GetLocation().compare(0, 8, "/system/") != 0) {
////    Dumper::Instance()->DumpClassFromDex(dex_file, *declaring_class->GetClassDef());
//    index = Dumper::Instance()->DumpEncodedFieldFromDex(dex_file,
//            field->GetDexFieldIndex(), INSTANCE, field->GetAccessFlags());
//  } else {
    index = Dumper::Instance()->DumpFieldFromDex(dex_file, field->GetDexFieldIndex());
//  }

  // LOG(ERROR) << "HandleFieldPutQuick FindFieldType " << field_type << " target field " << f->GetName() << ", " << index;
  // LOG(ERROR) << "HandleFieldPutQuick FindFieldType " << field_type << " src " << codes[0] << " " << codes[1];
  uint8_t src_opcode = codes[0] & 0xff;
  uint8_t target_opcode;
  if (src_opcode == 0xE6) {
    target_opcode = static_cast<uint8_t>(0x59);
  } else if (src_opcode == 0xE7) {
    target_opcode = static_cast<uint8_t>(0x5A);
  } else {
    target_opcode = static_cast<uint8_t>(0x5B);
  }
  uint16_t* new_inst = new uint16_t[count];
  memcpy(new_inst, codes, count * 2);
  new_inst[0] = (codes[0] & 0xff00) | target_opcode;
  new_inst[1] = static_cast<uint16_t>(index);
  // LOG(ERROR) << "HandleFieldGetQuick FindFieldType " << field_type << " dst " << new_inst[0] << " " << new_inst[1];
  // LOG(ERROR) << "HandleFieldPutQuick end";
  return new_inst;
}

// Explicitly instantiate all DoInvoke functions.
#define EXPLICIT_HANDLE_INVOKE_TEMPLATE_DECL(_type, _is_range, _do_check)                                         \
  template SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)                                                            \
  uint16_t* HandleInvoke<_type, _is_range, _do_check>(const uint16_t* code, const ShadowFrame& shadow_frame,     \
      ArtMethod* method, uint32_t count)

#define EXPLICIT_HANDLE_INVOKE_ALL_TEMPLATE_DECL(_type)       \
  EXPLICIT_HANDLE_INVOKE_TEMPLATE_DECL(_type, false, false);  \
  EXPLICIT_HANDLE_INVOKE_TEMPLATE_DECL(_type, false, true);   \
  EXPLICIT_HANDLE_INVOKE_TEMPLATE_DECL(_type, true, false);   \
  EXPLICIT_HANDLE_INVOKE_TEMPLATE_DECL(_type, true, true);

EXPLICIT_HANDLE_INVOKE_ALL_TEMPLATE_DECL(kStatic);      // invoke-static/range.
EXPLICIT_HANDLE_INVOKE_ALL_TEMPLATE_DECL(kDirect);      // invoke-direct/range.
EXPLICIT_HANDLE_INVOKE_ALL_TEMPLATE_DECL(kVirtual);     // invoke-virtual/range.
EXPLICIT_HANDLE_INVOKE_ALL_TEMPLATE_DECL(kSuper);       // invoke-super/range.
EXPLICIT_HANDLE_INVOKE_ALL_TEMPLATE_DECL(kInterface);   // invoke-interface/range.
#undef EXPLICIT_HANDLE_INVOKE_ALL_TEMPLATE_DECL
#undef EXPLICIT_HANDLE_INVOKE_TEMPLATE_DECL

// Explicitly instantiate all DoInvokeVirtualQuick functions.
#define EXPLICIT_HANDLE_INVOKE_VIRTUAL_QUICK_TEMPLATE_DECL(_is_range)                    \
  template SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)                               \
  uint16_t* HandleInvokeQuick<_is_range>(const uint16_t* code, const ShadowFrame& shadow_frame,      \
      ArtMethod* method, uint32_t count)

EXPLICIT_HANDLE_INVOKE_VIRTUAL_QUICK_TEMPLATE_DECL(false);  // invoke-virtual-quick.
EXPLICIT_HANDLE_INVOKE_VIRTUAL_QUICK_TEMPLATE_DECL(true);   // invoke-virtual-quick-range.
#undef EXPLICIT_INSTANTIATION_DO_INVOKE_VIRTUAL_QUICK

// Explicitly instantiate all DoFieldGet functions.
#define EXPLICIT_HANDLE_FIELD_GET_TEMPLATE_DECL(_find_type, _field_type, _do_check) \
  template uint16_t* HandleFieldGet<_find_type, _field_type, _do_check>(const uint16_t* code, \
      const ShadowFrame& shadow_frame, \
                                                               uint32_t count, ArtField* field, bool is_static)

#define EXPLICIT_HANDLE_FIELD_GET_ALL_TEMPLATE_DECL(_find_type, _field_type)  \
    EXPLICIT_HANDLE_FIELD_GET_TEMPLATE_DECL(_find_type, _field_type, false);  \
    EXPLICIT_HANDLE_FIELD_GET_TEMPLATE_DECL(_find_type, _field_type, true);

// iget-XXX
EXPLICIT_HANDLE_FIELD_GET_ALL_TEMPLATE_DECL(InstancePrimitiveRead, Primitive::kPrimBoolean);
EXPLICIT_HANDLE_FIELD_GET_ALL_TEMPLATE_DECL(InstancePrimitiveRead, Primitive::kPrimByte);
EXPLICIT_HANDLE_FIELD_GET_ALL_TEMPLATE_DECL(InstancePrimitiveRead, Primitive::kPrimChar);
EXPLICIT_HANDLE_FIELD_GET_ALL_TEMPLATE_DECL(InstancePrimitiveRead, Primitive::kPrimShort);
EXPLICIT_HANDLE_FIELD_GET_ALL_TEMPLATE_DECL(InstancePrimitiveRead, Primitive::kPrimInt);
EXPLICIT_HANDLE_FIELD_GET_ALL_TEMPLATE_DECL(InstancePrimitiveRead, Primitive::kPrimLong);
EXPLICIT_HANDLE_FIELD_GET_ALL_TEMPLATE_DECL(InstanceObjectRead, Primitive::kPrimNot);

// sget-XXX
EXPLICIT_HANDLE_FIELD_GET_ALL_TEMPLATE_DECL(StaticPrimitiveRead, Primitive::kPrimBoolean);
EXPLICIT_HANDLE_FIELD_GET_ALL_TEMPLATE_DECL(StaticPrimitiveRead, Primitive::kPrimByte);
EXPLICIT_HANDLE_FIELD_GET_ALL_TEMPLATE_DECL(StaticPrimitiveRead, Primitive::kPrimChar);
EXPLICIT_HANDLE_FIELD_GET_ALL_TEMPLATE_DECL(StaticPrimitiveRead, Primitive::kPrimShort);
EXPLICIT_HANDLE_FIELD_GET_ALL_TEMPLATE_DECL(StaticPrimitiveRead, Primitive::kPrimInt);
EXPLICIT_HANDLE_FIELD_GET_ALL_TEMPLATE_DECL(StaticPrimitiveRead, Primitive::kPrimLong);
EXPLICIT_HANDLE_FIELD_GET_ALL_TEMPLATE_DECL(StaticObjectRead, Primitive::kPrimNot);

#undef EXPLICIT_HANDLE_FIELD_GET_ALL_TEMPLATE_DECL
#undef EXPLICIT_HANDLE_FIELD_GET_TEMPLATE_DECL

// Explicitly instantiate all DoIGetQuick functions.
#define EXPLICIT_HANDLE_IGET_QUICK_TEMPLATE_DECL(_field_type) \
  template uint16_t* HandleFieldGetQuick<_field_type>(const uint16_t* code, const ShadowFrame& shadow_frame, ArtField* field, uint32_t count)

EXPLICIT_HANDLE_IGET_QUICK_TEMPLATE_DECL(Primitive::kPrimInt);    // iget-quick.
EXPLICIT_HANDLE_IGET_QUICK_TEMPLATE_DECL(Primitive::kPrimLong);   // iget-wide-quick.
EXPLICIT_HANDLE_IGET_QUICK_TEMPLATE_DECL(Primitive::kPrimNot);    // iget-object-quick.
EXPLICIT_HANDLE_IGET_QUICK_TEMPLATE_DECL(Primitive::kPrimBoolean);  // iget-boolean-quick.
EXPLICIT_HANDLE_IGET_QUICK_TEMPLATE_DECL(Primitive::kPrimByte);  // iget-byte-quick.
EXPLICIT_HANDLE_IGET_QUICK_TEMPLATE_DECL(Primitive::kPrimChar);  // iget-char-quick.
EXPLICIT_HANDLE_IGET_QUICK_TEMPLATE_DECL(Primitive::kPrimShort);  // iget-short-quick.
#undef EXPLICIT_HANDLE_IGET_QUICK_TEMPLATE_DECL

// Explicitly instantiate all DoFieldPut functions.
#define EXPLICIT_HANDLE_FIELD_PUT_TEMPLATE_DECL(_find_type, _field_type, _do_check) \
  template uint16_t* HandleFieldPut<_find_type, _field_type, _do_check>(const uint16_t* code, \
      const ShadowFrame& shadow_frame, uint32_t count, ArtField* field, bool is_static)

#define EXPLICIT_HANDLE_FIELD_PUT_ALL_TEMPLATE_DECL(_find_type, _field_type)  \
    EXPLICIT_HANDLE_FIELD_PUT_TEMPLATE_DECL(_find_type, _field_type, false);  \
    EXPLICIT_HANDLE_FIELD_PUT_TEMPLATE_DECL(_find_type, _field_type, true);  \

// iput-XXX
EXPLICIT_HANDLE_FIELD_PUT_ALL_TEMPLATE_DECL(InstancePrimitiveWrite, Primitive::kPrimBoolean);
EXPLICIT_HANDLE_FIELD_PUT_ALL_TEMPLATE_DECL(InstancePrimitiveWrite, Primitive::kPrimByte);
EXPLICIT_HANDLE_FIELD_PUT_ALL_TEMPLATE_DECL(InstancePrimitiveWrite, Primitive::kPrimChar);
EXPLICIT_HANDLE_FIELD_PUT_ALL_TEMPLATE_DECL(InstancePrimitiveWrite, Primitive::kPrimShort);
EXPLICIT_HANDLE_FIELD_PUT_ALL_TEMPLATE_DECL(InstancePrimitiveWrite, Primitive::kPrimInt);
EXPLICIT_HANDLE_FIELD_PUT_ALL_TEMPLATE_DECL(InstancePrimitiveWrite, Primitive::kPrimLong);
EXPLICIT_HANDLE_FIELD_PUT_ALL_TEMPLATE_DECL(InstanceObjectWrite, Primitive::kPrimNot);

// sput-XXX
EXPLICIT_HANDLE_FIELD_PUT_ALL_TEMPLATE_DECL(StaticPrimitiveWrite, Primitive::kPrimBoolean);
EXPLICIT_HANDLE_FIELD_PUT_ALL_TEMPLATE_DECL(StaticPrimitiveWrite, Primitive::kPrimByte);
EXPLICIT_HANDLE_FIELD_PUT_ALL_TEMPLATE_DECL(StaticPrimitiveWrite, Primitive::kPrimChar);
EXPLICIT_HANDLE_FIELD_PUT_ALL_TEMPLATE_DECL(StaticPrimitiveWrite, Primitive::kPrimShort);
EXPLICIT_HANDLE_FIELD_PUT_ALL_TEMPLATE_DECL(StaticPrimitiveWrite, Primitive::kPrimInt);
EXPLICIT_HANDLE_FIELD_PUT_ALL_TEMPLATE_DECL(StaticPrimitiveWrite, Primitive::kPrimLong);
EXPLICIT_HANDLE_FIELD_PUT_ALL_TEMPLATE_DECL(StaticObjectWrite, Primitive::kPrimNot);

#undef EXPLICIT_HANDLE_FIELD_PUT_ALL_TEMPLATE_DECL
#undef EXPLICIT_HANDLE_FIELD_PUT_TEMPLATE_DECL

// Explicitly instantiate all DoIPutQuick functions.
#define EXPLICIT_HANDLE_IPUT_QUICK_TEMPLATE_DECL(_field_type) \
  template uint16_t* HandleFieldPutQuick<_field_type>(const uint16_t* code, const ShadowFrame& shadow_frame, \
      ArtField* field, uint32_t count)

EXPLICIT_HANDLE_IPUT_QUICK_TEMPLATE_DECL(Primitive::kPrimInt);    // iget-quick.
EXPLICIT_HANDLE_IPUT_QUICK_TEMPLATE_DECL(Primitive::kPrimLong);   // iget-wide-quick.
EXPLICIT_HANDLE_IPUT_QUICK_TEMPLATE_DECL(Primitive::kPrimNot);    // iget-object-quick.
EXPLICIT_HANDLE_IPUT_QUICK_TEMPLATE_DECL(Primitive::kPrimBoolean);  // iget-boolean-quick.
EXPLICIT_HANDLE_IPUT_QUICK_TEMPLATE_DECL(Primitive::kPrimByte);  // iget-byte-quick.
EXPLICIT_HANDLE_IPUT_QUICK_TEMPLATE_DECL(Primitive::kPrimChar);  // iget-char-quick.
EXPLICIT_HANDLE_IPUT_QUICK_TEMPLATE_DECL(Primitive::kPrimShort);  // iget-short-quick.
#undef EXPLICIT_HANDLE_IPUT_QUICK_TEMPLATE_DECL

}  // namespace art

#endif  // ART_RUNTIME_UNPACK_DUMP_HANDLE_H_
