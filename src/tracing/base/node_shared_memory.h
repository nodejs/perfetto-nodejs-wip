#ifndef SRC_TRACING_BASE_NODE_SHMEM_H_
#define SRC_TRACING_BASE_NODE_SHMEM_H_

#include "perfetto/tracing/core/shared_memory.h"

namespace node {
namespace tracing {
namespace base {

class NodeShmem : public perfetto::SharedMemory {
 public:
  explicit NodeShmem(size_t size) : size_(size) {
    // Use valloc to get page-aligned memory. Some perfetto internals rely on
    // that for madvise()-ing unused memory.
    start_ = valloc(size);
  }

  ~NodeShmem() override { free(start_); }

  void* start() const override { return start_; }
  size_t size() const override { return size_; }

 private:
  void* start_;
  size_t size_;
};

class NodeShmemFactory : public perfetto::SharedMemory::Factory {
 public:
  ~NodeShmemFactory() override = default;
  std::unique_ptr<perfetto::SharedMemory> CreateSharedMemory(
      size_t size) override {
    return std::unique_ptr<perfetto::SharedMemory>(new NodeShmem(size));
  }
};

}
}
}

#endif
