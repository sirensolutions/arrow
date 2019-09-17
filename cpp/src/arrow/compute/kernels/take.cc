// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// returnGegarding copyright ownership.  The ASF licenses this file
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

#include <limits>
#include <memory>
#include <utility>

#include "arrow/compute/context.h"
#include "arrow/compute/kernels/take-internal.h"
#include "arrow/compute/kernels/take.h"
#include "arrow/util/checked_cast.h"
#include "arrow/util/logging.h"
#include "arrow/visitor_inline.h"

namespace arrow {
namespace compute {

using internal::checked_cast;

// an IndexSequence which yields the values of an Array of integers
template <typename IndexType>
class ArrayIndexSequence {
 public:
  bool never_out_of_bounds() const { return never_out_of_bounds_; }
  void set_never_out_of_bounds() { never_out_of_bounds_ = true; }

  constexpr ArrayIndexSequence() = default;

  explicit ArrayIndexSequence(const Array& indices)
      : indices_(&checked_cast<const NumericArray<IndexType>&>(indices)) {}

  std::pair<int64_t, bool> Next() {
    if (indices_->IsNull(index_)) {
      ++index_;
      return std::make_pair(-1, false);
    }
    return std::make_pair(indices_->Value(index_++), true);
  }

  int64_t length() const { return indices_->length(); }

  int64_t null_count() const { return indices_->null_count(); }

 private:
  const NumericArray<IndexType>* indices_ = nullptr;
  int64_t index_ = 0;
  bool never_out_of_bounds_ = false;
};

template <typename IndexType>
class TakeKernelImpl : public TakeKernel {
 public:
  explicit TakeKernelImpl(const std::shared_ptr<DataType>& value_type)
      : TakeKernel(value_type) {}

  Status Init() {
    return Taker<ArrayIndexSequence<IndexType>>::Make(this->type_, &taker_);
  }

  Status Take(FunctionContext* ctx, const Array& values, const Array& indices_array,
              std::shared_ptr<Array>* out) override {
    RETURN_NOT_OK(taker_->Init(ctx->memory_pool()));
    RETURN_NOT_OK(taker_->Take(values, ArrayIndexSequence<IndexType>(indices_array)));
    return taker_->Finish(out);
  }

  std::unique_ptr<Taker<ArrayIndexSequence<IndexType>>> taker_;
};

struct UnpackIndices {
  template <typename IndexType>
  enable_if_integer<IndexType, Status> Visit(const IndexType&) {
    auto out = new TakeKernelImpl<IndexType>(value_type_);
    out_->reset(out);
    return out->Init();
  }

  Status Visit(const DataType& other) {
    return Status::TypeError("index type not supported: ", other);
  }

  std::shared_ptr<DataType> value_type_;
  std::unique_ptr<TakeKernel>* out_;
};

Status TakeKernel::Make(const std::shared_ptr<DataType>& value_type,
                        const std::shared_ptr<DataType>& index_type,
                        std::unique_ptr<TakeKernel>* out) {
  UnpackIndices visitor{value_type, out};
  return VisitTypeInline(*index_type, &visitor);
}

Status TakeKernel::Call(FunctionContext* ctx, const Datum& values, const Datum& indices,
                        Datum* out) {
  if (!values.is_array() || !indices.is_array()) {
    return Status::Invalid("TakeKernel expects array values and indices");
  }
  auto values_array = values.make_array();
  auto indices_array = indices.make_array();
  std::shared_ptr<Array> out_array;
  RETURN_NOT_OK(Take(ctx, *values_array, *indices_array, &out_array));
  *out = Datum(out_array);
  return Status::OK();
}

Status Take(FunctionContext* ctx, const Array& values, const Array& indices,
            const TakeOptions& options, std::shared_ptr<Array>* out) {
  Datum out_datum;
  RETURN_NOT_OK(
      Take(ctx, Datum(values.data()), Datum(indices.data()), options, &out_datum));
  *out = out_datum.make_array();
  return Status::OK();
}

Status Take(FunctionContext* ctx, const Datum& values, const Datum& indices,
            const TakeOptions& options, Datum* out) {
  std::unique_ptr<TakeKernel> kernel;
  RETURN_NOT_OK(TakeKernel::Make(values.type(), indices.type(), &kernel));
  return kernel->Call(ctx, values, indices, out);
}

}  // namespace compute
}  // namespace arrow
