#ifndef SRC_TRACING_BASE_NODE_PRODUCER_ENDPOINT_H_
#define SRC_TRACING_BASE_NODE_PRODUCER_ENDPOINT_H_

#include "node_mutex.h"
#include "perfetto/tracing/core/tracing_service.h"
#include "perfetto/base/task_runner.h"
#include "perfetto/tracing/core/data_source_descriptor.h"
#include "perfetto/tracing/core/shared_memory.h"
#include "perfetto/tracing/core/trace_writer.h"
#include "perfetto/tracing/core/commit_data_request.h"

namespace node {
namespace tracing {
namespace base {

/**
 * Wraps an existing producer endpoint to provide a "fake IPC layer" in which
 * API calls will be scheduled on the given task runner rather than invoked
 * immediately.
 */
class NodeProducerEndpoint : public perfetto::TracingService::ProducerEndpoint {
 public:
  NodeProducerEndpoint(
    std::unique_ptr<perfetto::TracingService::ProducerEndpoint> svc_endpoint,
    std::shared_ptr<perfetto::base::TaskRunner> task_runner)
    : svc_endpoint_(std::move(svc_endpoint)), task_runner_(task_runner) {}
  ~NodeProducerEndpoint();
  void RegisterDataSource(const perfetto::DataSourceDescriptor&) override;
  void UnregisterDataSource(const std::string& name) override;
  void RegisterTraceWriter(uint32_t writer_id, uint32_t target_buffer) override;
  void UnregisterTraceWriter(uint32_t writer_id) override;
  using CommitDataCallback = std::function<void()>;
  void CommitData(const perfetto::CommitDataRequest&, CommitDataCallback callback = {}) override;
  std::unique_ptr<perfetto::TraceWriter> CreateTraceWriter(perfetto::BufferID target_buffer) override;
  void NotifyFlushComplete(perfetto::FlushRequestID) override;
  void NotifyDataSourceStarted(perfetto::DataSourceInstanceID) override;
  void NotifyDataSourceStopped(perfetto::DataSourceInstanceID) override;
  void ActivateTriggers(const std::vector<std::string>&) override;

  perfetto::SharedMemory* shared_memory() const override {
    return svc_endpoint_->shared_memory();
  }

  size_t shared_buffer_page_size_kb() const override {
    return svc_endpoint_->shared_buffer_page_size_kb();
  }
 private:
  std::unique_ptr<perfetto::TracingService::ProducerEndpoint> svc_endpoint_;
  std::shared_ptr<perfetto::base::TaskRunner> task_runner_;
};

}
}
}

#endif
