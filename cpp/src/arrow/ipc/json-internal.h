// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#ifndef ARROW_IPC_JSON_INTERNAL_H
#define ARROW_IPC_JSON_INTERNAL_H

#define RAPIDJSON_HAS_STDSTRING 1
#define RAPIDJSON_HAS_CXX11_RVALUE_REFS 1
#define RAPIDJSON_HAS_CXX11_RANGE_FOR 1

#include <memory>
#include <sstream>
#include <string>

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include "arrow/type_fwd.h"  // IWYU pragma: export
#include "arrow/util/visibility.h"

namespace rj = rapidjson;
using RjWriter = rj::Writer<rj::StringBuffer>;
using RjArray = rj::Value::ConstArray;
using RjObject = rj::Value::ConstObject;

#define RETURN_NOT_FOUND(TOK, NAME, PARENT) \
  if (NAME == (PARENT).MemberEnd()) {       \
    std::stringstream ss;                   \
    ss << "field " << TOK << " not found";  \
    return Status::Invalid(ss.str());       \
  }

#define RETURN_NOT_STRING(TOK, NAME, PARENT) \
  RETURN_NOT_FOUND(TOK, NAME, PARENT);       \
  if (!NAME->value.IsString()) {             \
    std::stringstream ss;                    \
    ss << "field was not a string"           \
       << " line " << __LINE__;              \
    return Status::Invalid(ss.str());        \
  }

#define RETURN_NOT_BOOL(TOK, NAME, PARENT) \
  RETURN_NOT_FOUND(TOK, NAME, PARENT);     \
  if (!NAME->value.IsBool()) {             \
    std::stringstream ss;                  \
    ss << "field was not a boolean"        \
       << " line " << __LINE__;            \
    return Status::Invalid(ss.str());      \
  }

#define RETURN_NOT_INT(TOK, NAME, PARENT) \
  RETURN_NOT_FOUND(TOK, NAME, PARENT);    \
  if (!NAME->value.IsInt()) {             \
    std::stringstream ss;                 \
    ss << "field was not an int"          \
       << " line " << __LINE__;           \
    return Status::Invalid(ss.str());     \
  }

#define RETURN_NOT_ARRAY(TOK, NAME, PARENT) \
  RETURN_NOT_FOUND(TOK, NAME, PARENT);      \
  if (!NAME->value.IsArray()) {             \
    std::stringstream ss;                   \
    ss << "field was not an array"          \
       << " line " << __LINE__;             \
    return Status::Invalid(ss.str());       \
  }

#define RETURN_NOT_OBJECT(TOK, NAME, PARENT) \
  RETURN_NOT_FOUND(TOK, NAME, PARENT);       \
  if (!NAME->value.IsObject()) {             \
    std::stringstream ss;                    \
    ss << "field was not an object"          \
       << " line " << __LINE__;              \
    return Status::Invalid(ss.str());        \
  }

namespace arrow {
namespace ipc {
namespace internal {
namespace json {

Status WriteSchema(const Schema& schema, RjWriter* writer);
Status WriteRecordBatch(const RecordBatch& batch, RjWriter* writer);
Status WriteArray(const std::string& name, const Array& array, RjWriter* writer);

Status ReadSchema(const rj::Value& json_obj, MemoryPool* pool,
                  std::shared_ptr<Schema>* schema);

Status ReadRecordBatch(const rj::Value& json_obj, const std::shared_ptr<Schema>& schema,
                       MemoryPool* pool, std::shared_ptr<RecordBatch>* batch);

Status ReadArray(MemoryPool* pool, const rj::Value& json_obj,
                 const std::shared_ptr<DataType>& type, std::shared_ptr<Array>* array);

Status ReadArray(MemoryPool* pool, const rj::Value& json_obj, const Schema& schema,
                 std::shared_ptr<Array>* array);

}  // namespace json
}  // namespace internal
}  // namespace ipc
}  // namespace arrow

#endif  // ARROW_IPC_JSON_INTERNAL_H
