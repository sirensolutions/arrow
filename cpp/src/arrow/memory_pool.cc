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

#include "arrow/memory_pool.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <sstream>  // IWYU pragma: keep

#include "arrow/status.h"
#include "arrow/util/logging.h"

#ifdef ARROW_JEMALLOC
// Needed to support jemalloc 3 and 4
#define JEMALLOC_MANGLE
#include <jemalloc/jemalloc.h>
#endif

namespace arrow {

constexpr size_t kAlignment = 64;

namespace {
// Allocate memory according to the alignment requirements for Arrow
// (as of May 2016 64 bytes)
Status AllocateAligned(int64_t size, uint8_t** out) {
// TODO(emkornfield) find something compatible with windows
#ifdef _MSC_VER
  // Special code path for MSVC
  *out =
      reinterpret_cast<uint8_t*>(_aligned_malloc(static_cast<size_t>(size), kAlignment));
  if (!*out) {
    std::stringstream ss;
    ss << "malloc of size " << size << " failed";
    return Status::OutOfMemory(ss.str());
  }
#elif defined(ARROW_JEMALLOC)
  *out = reinterpret_cast<uint8_t*>(mallocx(
      std::max(static_cast<size_t>(size), kAlignment), MALLOCX_ALIGN(kAlignment)));
  if (*out == NULL) {
    std::stringstream ss;
    ss << "malloc of size " << size << " failed";
    return Status::OutOfMemory(ss.str());
  }
#else
  const int result = posix_memalign(reinterpret_cast<void**>(out), kAlignment,
                                    static_cast<size_t>(size));
  if (result == ENOMEM) {
    std::stringstream ss;
    ss << "malloc of size " << size << " failed";
    return Status::OutOfMemory(ss.str());
  }

  if (result == EINVAL) {
    std::stringstream ss;
    ss << "invalid alignment parameter: " << kAlignment;
    return Status::Invalid(ss.str());
  }
#endif
  return Status::OK();
}
}  // namespace

MemoryPool::MemoryPool() {}

MemoryPool::~MemoryPool() {}

int64_t MemoryPool::max_memory() const { return -1; }

class DefaultMemoryPool : public MemoryPool {
 public:
  DefaultMemoryPool() : bytes_allocated_(0) { max_memory_ = 0; }

  ~DefaultMemoryPool() {}

  Status Allocate(int64_t size, uint8_t** out) override {
    RETURN_NOT_OK(AllocateAligned(size, out));
    bytes_allocated_ += size;

    {
      std::lock_guard<std::mutex> guard(lock_);
      if (bytes_allocated_ > max_memory_) {
        max_memory_ = bytes_allocated_.load();
      }
    }
    return Status::OK();
  }

  Status Reallocate(int64_t old_size, int64_t new_size, uint8_t** ptr) override {
#ifdef ARROW_JEMALLOC
    *ptr = reinterpret_cast<uint8_t*>(rallocx(*ptr, new_size, MALLOCX_ALIGN(kAlignment)));
    if (*ptr == NULL) {
      std::stringstream ss;
      ss << "realloc of size " << new_size << " failed";
      return Status::OutOfMemory(ss.str());
    }
#else
    // Note: We cannot use realloc() here as it doesn't guarantee alignment.

    // Allocate new chunk
    uint8_t* out = nullptr;
    RETURN_NOT_OK(AllocateAligned(new_size, &out));
    DCHECK(out);
    // Copy contents and release old memory chunk
    memcpy(out, *ptr, static_cast<size_t>(std::min(new_size, old_size)));
#ifdef _MSC_VER
    _aligned_free(*ptr);
#else
    std::free(*ptr);
#endif  // defined(_MSC_VER)
    *ptr = out;
#endif  // defined(ARROW_JEMALLOC)

    bytes_allocated_ += new_size - old_size;
    {
      std::lock_guard<std::mutex> guard(lock_);
      if (bytes_allocated_ > max_memory_) {
        max_memory_ = bytes_allocated_.load();
      }
    }

    return Status::OK();
  }

  int64_t bytes_allocated() const override { return bytes_allocated_.load(); }

  void Free(uint8_t* buffer, int64_t size) override {
    DCHECK_GE(bytes_allocated_, size);
#ifdef _MSC_VER
    _aligned_free(buffer);
#elif defined(ARROW_JEMALLOC)
    dallocx(buffer, MALLOCX_ALIGN(kAlignment));
#else
    std::free(buffer);
#endif
    bytes_allocated_ -= size;
  }

  int64_t max_memory() const override { return max_memory_.load(); }

 private:
  mutable std::mutex lock_;
  std::atomic<int64_t> bytes_allocated_;
  std::atomic<int64_t> max_memory_;
};

MemoryPool* default_memory_pool() {
  static DefaultMemoryPool default_memory_pool_;
  return &default_memory_pool_;
}

LoggingMemoryPool::LoggingMemoryPool(MemoryPool* pool) : pool_(pool) {}

Status LoggingMemoryPool::Allocate(int64_t size, uint8_t** out) {
  Status s = pool_->Allocate(size, out);
  std::cout << "Allocate: size = " << size << std::endl;
  return s;
}

Status LoggingMemoryPool::Reallocate(int64_t old_size, int64_t new_size, uint8_t** ptr) {
  Status s = pool_->Reallocate(old_size, new_size, ptr);
  std::cout << "Reallocate: old_size = " << old_size << " - new_size = " << new_size
            << std::endl;
  return s;
}

void LoggingMemoryPool::Free(uint8_t* buffer, int64_t size) {
  pool_->Free(buffer, size);
  std::cout << "Free: size = " << size << std::endl;
}

int64_t LoggingMemoryPool::bytes_allocated() const {
  int64_t nb_bytes = pool_->bytes_allocated();
  std::cout << "bytes_allocated: " << nb_bytes << std::endl;
  return nb_bytes;
}

int64_t LoggingMemoryPool::max_memory() const {
  int64_t mem = pool_->max_memory();
  std::cout << "max_memory: " << mem << std::endl;
  return mem;
}
}  // namespace arrow
