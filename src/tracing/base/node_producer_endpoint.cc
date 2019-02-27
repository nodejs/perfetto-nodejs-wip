#include "tracing/base/node_producer_endpoint.h"

namespace node {
namespace tracing {
namespace base {

using CommitDataCallback = std::function<void()>;

NodeProducerEndpoint::~NodeProducerEndpoint() {}

void NodeProducerEndpoint::RegisterDataSource(const perfetto::DataSourceDescriptor& data_source) {
  task_runner_->PostTask([=]() {
    svc_endpoint_->RegisterDataSource(data_source);
  });
}

void NodeProducerEndpoint::UnregisterDataSource(const std::string& name) {
  task_runner_->PostTask([=]() {
    svc_endpoint_->UnregisterDataSource(name);
  });
}

void NodeProducerEndpoint::RegisterTraceWriter(uint32_t writer_id, uint32_t target_buffer) {
  task_runner_->PostTask([=]() {
    svc_endpoint_->RegisterTraceWriter(writer_id, target_buffer);
  });
}

void NodeProducerEndpoint::UnregisterTraceWriter(uint32_t writer_id) {
  task_runner_->PostTask([=]() {
    svc_endpoint_->UnregisterTraceWriter(writer_id);
  });
}

void NodeProducerEndpoint::CommitData(const perfetto::CommitDataRequest& commit_data_request, CommitDataCallback callback) {
  task_runner_->PostTask([=]() {
    svc_endpoint_->CommitData(commit_data_request, callback);
  });
}

std::unique_ptr<perfetto::TraceWriter> NodeProducerEndpoint::CreateTraceWriter(perfetto::BufferID target_buffer) {
  // CreateTraceWriter can be called from any endpoint, so call it right away.
  return svc_endpoint_->CreateTraceWriter(target_buffer);
}

void NodeProducerEndpoint::NotifyFlushComplete(perfetto::FlushRequestID flush_request_id) {
  task_runner_->PostTask([=]() {
    svc_endpoint_->NotifyFlushComplete(flush_request_id);
  });
}

void NodeProducerEndpoint::NotifyDataSourceStarted(perfetto::DataSourceInstanceID data_source_instance_id) {
  task_runner_->PostTask([=]() {
    svc_endpoint_->NotifyDataSourceStarted(data_source_instance_id);
  });
}

void NodeProducerEndpoint::NotifyDataSourceStopped(perfetto::DataSourceInstanceID data_source_instance_id) {
  task_runner_->PostTask([=]() {
    svc_endpoint_->NotifyDataSourceStopped(data_source_instance_id);
  });
}

void NodeProducerEndpoint::ActivateTriggers(const std::vector<std::string>& triggers) {
  std::vector<std::string> captured_triggers(triggers);
  task_runner_->PostTask([=]() {
    svc_endpoint_->ActivateTriggers(captured_triggers);
  });
}

}
}
}
