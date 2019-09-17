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

#include "arrow/scalar.h"

#include <memory>

#include "arrow/array.h"
#include "arrow/buffer.h"
#include "arrow/compare.h"
#include "arrow/type.h"
#include "arrow/util/checked_cast.h"
#include "arrow/util/decimal.h"
#include "arrow/util/logging.h"

namespace arrow {

using internal::checked_cast;

bool Scalar::Equals(const Scalar& other) const { return ScalarEquals(*this, other); }

Time32Scalar::Time32Scalar(int32_t value, const std::shared_ptr<DataType>& type,
                           bool is_valid)
    : internal::PrimitiveScalar{type, is_valid}, value(value) {
  ARROW_CHECK_EQ(Type::TIME32, type->id());
}

Time64Scalar::Time64Scalar(int64_t value, const std::shared_ptr<DataType>& type,
                           bool is_valid)
    : internal::PrimitiveScalar{type, is_valid}, value(value) {
  ARROW_CHECK_EQ(Type::TIME64, type->id());
}

TimestampScalar::TimestampScalar(int64_t value, const std::shared_ptr<DataType>& type,
                                 bool is_valid)
    : internal::PrimitiveScalar{type, is_valid}, value(value) {
  ARROW_CHECK_EQ(Type::TIMESTAMP, type->id());
}

DurationScalar::DurationScalar(int64_t value, const std::shared_ptr<DataType>& type,
                               bool is_valid)
    : internal::PrimitiveScalar{type, is_valid}, value(value) {
  DCHECK_EQ(Type::DURATION, type->id());
}

MonthIntervalScalar::MonthIntervalScalar(int32_t value,
                                         const std::shared_ptr<DataType>& type,
                                         bool is_valid)
    : internal::PrimitiveScalar{type, is_valid}, value(value) {
  DCHECK_EQ(Type::INTERVAL, type->id());
  DCHECK_EQ(IntervalType::MONTHS,
            checked_cast<IntervalType*>(type.get())->interval_type());
}

DayTimeIntervalScalar::DayTimeIntervalScalar(DayTimeIntervalType::DayMilliseconds value,
                                             const std::shared_ptr<DataType>& type,
                                             bool is_valid)
    : internal::PrimitiveScalar{type, is_valid}, value(value) {
  DCHECK_EQ(Type::INTERVAL, type->id());
  DCHECK_EQ(IntervalType::DAY_TIME,
            checked_cast<IntervalType*>(type.get())->interval_type());
}

FixedSizeBinaryScalar::FixedSizeBinaryScalar(const std::shared_ptr<Buffer>& value,
                                             const std::shared_ptr<DataType>& type,
                                             bool is_valid)
    : BinaryScalar(value, type, is_valid) {
  ARROW_CHECK_EQ(checked_cast<const FixedSizeBinaryType&>(*type).byte_width(),
                 value->size());
}

Decimal128Scalar::Decimal128Scalar(const Decimal128& value,
                                   const std::shared_ptr<DataType>& type, bool is_valid)
    : Scalar{type, is_valid}, value(value) {}

ListScalar::ListScalar(const std::shared_ptr<Array>& value,
                       const std::shared_ptr<DataType>& type, bool is_valid)
    : Scalar{type, is_valid}, value(value) {}

ListScalar::ListScalar(const std::shared_ptr<Array>& value, bool is_valid)
    : ListScalar(value, value->type(), is_valid) {}

MapScalar::MapScalar(const std::shared_ptr<Array>& keys,
                     const std::shared_ptr<Array>& items,
                     const std::shared_ptr<DataType>& type, bool is_valid)
    : Scalar{type, is_valid}, keys(keys), items(items) {}

MapScalar::MapScalar(const std::shared_ptr<Array>& keys,
                     const std::shared_ptr<Array>& values, bool is_valid)
    : MapScalar(keys, values, map(keys->type(), values->type()), is_valid) {}

FixedSizeListScalar::FixedSizeListScalar(const std::shared_ptr<Array>& value,
                                         const std::shared_ptr<DataType>& type,
                                         bool is_valid)
    : Scalar{type, is_valid}, value(value) {
  ARROW_CHECK_EQ(value->length(),
                 checked_cast<const FixedSizeListType*>(type.get())->list_size());
}

FixedSizeListScalar::FixedSizeListScalar(const std::shared_ptr<Array>& value,
                                         bool is_valid)
    : FixedSizeListScalar(value, value->type(), is_valid) {}

}  // namespace arrow
