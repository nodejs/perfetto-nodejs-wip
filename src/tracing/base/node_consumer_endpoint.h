#ifndef SRC_TRACING_BASE_NODE_CONSUMER_ENDPOINT_H_
#define SRC_TRACING_BASE_NODE_CONSUMER_ENDPOINT_H_

#include "node_mutex.h"
#include "perfetto/tracing/core/tracing_service.h"
#include "perfetto/tracing/core/trace_config.h"
#include "perfetto/base/task_runner.h"
#include "perfetto/base/scoped_file.h"

namespace node {
namespace tracing {
namespace base {

/**
 * Wraps an existing consumer endpoint to provide a "fake IPC layer" in which
 * API calls will be scheduled on the given task runner rather than invoked
 * immediately.
 */
class NodeConsumerEndpoint : public perfetto::TracingService::ConsumerEndpoint {
 public:
  NodeConsumerEndpoint(
    std::unique_ptr<perfetto::TracingService::ConsumerEndpoint> svc_endpoint,
    std::shared_ptr<perfetto::base::TaskRunner> task_runner)
    : svc_endpoint_(std::move(svc_endpoint)), task_runner_(task_runner) {}
  ~NodeConsumerEndpoint();
  void EnableTracing(const perfetto::TraceConfig&,
                     perfetto::base::ScopedFile = perfetto::base::ScopedFile()) override;
  void ChangeTraceConfig(const perfetto::TraceConfig&) override;
  void StartTracing() override;
  void DisableTracing() override;
  using FlushCallback = std::function<void(bool /*success*/)>;
  void Flush(uint32_t timeout_ms, FlushCallback) override;
  void ReadBuffers() override;
  void FreeBuffers() override;
  void Detach(const std::string& key) override;
  void Attach(const std::string& key) override;
  void GetTraceStats() override;
  void ObserveEvents(uint32_t enabled_event_types) override;
 private:
  std::unique_ptr<perfetto::TracingService::ConsumerEndpoint> svc_endpoint_;
  std::shared_ptr<perfetto::base::TaskRunner> task_runner_;
};

}
}
}

#endif
