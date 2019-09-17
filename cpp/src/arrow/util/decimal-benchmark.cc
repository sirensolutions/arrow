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

#include "benchmark/benchmark.h"

#include <string>
#include <vector>

#include "arrow/util/decimal.h"
#include "arrow/util/macros.h"

namespace arrow {
namespace Decimal {

static void FromString(benchmark::State& state) {  // NOLINT non-const reference
  std::vector<std::string> values = {"0",
                                     "1.23",
                                     "12.345e6",
                                     "-12.345e-6",
                                     "123456789.123456789",
                                     "1231234567890.451234567890"};

  while (state.KeepRunning()) {
    for (const auto& value : values) {
      Decimal128 dec;
      int32_t scale, precision;
      benchmark::DoNotOptimize(Decimal128::FromString(value, &dec, &scale, &precision));
    }
  }
  state.SetItemsProcessed(state.iterations() * values.size());
}

static void BinaryCompareOp(benchmark::State& state) {  // NOLINT non-const reference
  BasicDecimal128 d1(546, 123), d2(123, 456);
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(d1 == d2);
    benchmark::DoNotOptimize(d1 <= d2);
    benchmark::DoNotOptimize(d1 >= d2);
    benchmark::DoNotOptimize(d1 >= d1);
  }
}

static void BinaryMathOp(benchmark::State& state) {  // NOLINT non-const reference
  BasicDecimal128 d1(546, 123), d2(123, 456), d3(0, 10);
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(d1 - d2);
    benchmark::DoNotOptimize(d1 + d2);
    benchmark::DoNotOptimize(d1 * d2);
    benchmark::DoNotOptimize(d1 / d2);
    benchmark::DoNotOptimize(d1 % d3);
  }
}

static void UnaryOp(benchmark::State& state) {  // NOLINT non-const reference
  BasicDecimal128 d1(-546, 123), d2(-123, 456);
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(d1.Abs());
    benchmark::DoNotOptimize(d2.Negate());
  }
}

static void Constants(benchmark::State& state) {  // NOLINT non-const reference
  BasicDecimal128 d1(-546, 123), d2(-123, 456);
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(BasicDecimal128::GetMaxValue() - d1);
    benchmark::DoNotOptimize(BasicDecimal128::GetScaleMultiplier(3) + d2);
  }
}

static void BinaryBitOp(benchmark::State& state) {  // NOLINT non-const reference
  BasicDecimal128 d1(546, 123), d2(123, 456);
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(d1 |= d2);
    benchmark::DoNotOptimize(d1 &= d2);
  }
}

BENCHMARK(FromString);
BENCHMARK(BinaryMathOp);
BENCHMARK(BinaryCompareOp);
BENCHMARK(BinaryBitOp);
BENCHMARK(UnaryOp);
BENCHMARK(Constants);

}  // namespace Decimal
}  // namespace arrow
