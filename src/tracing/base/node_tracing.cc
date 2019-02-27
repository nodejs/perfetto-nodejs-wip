#include "tracing/base/node_tracing.h"
#include "tracing/base/node_shared_memory.h"

namespace node {
namespace tracing {
namespace base {

NodeTracing::NodeTracing()
  : task_runner_(new DelayedNodeTaskRunner()) {
  tracing_service_ = perfetto::TracingService::CreateInstance(
    std::unique_ptr<perfetto::SharedMemory::Factory>(new NodeShmemFactory()),
    task_runner_.get()
  );
}

NodeTracing::~NodeTracing() {
  for (auto itr = producers_.begin(); itr != producers_.end(); itr++) {
    if (!itr->expired()) {
      itr->lock()->svc_endpoint_.reset();
    }
  }
  for (auto itr = consumers_.begin(); itr != consumers_.end(); itr++) {
    if (!itr->expired()) {
      itr->lock()->svc_endpoint_.reset();
    }
  }
  tracing_service_.reset(nullptr);
}

void NodeTracing::Initialize() {
  task_runner_->Start();
}

void NodeTracing::ConnectConsumer(std::shared_ptr<NodeConsumer> consumer) {
  auto endpoint = tracing_service_->ConnectConsumer(consumer.get(), 0);
  consumer->svc_endpoint_.reset(new NodeConsumerEndpoint(std::move(endpoint), task_runner_));
  consumers_.push_back(consumer);
}

void NodeTracing::ConnectProducer(std::shared_ptr<NodeProducer> producer, std::string name) {
  auto endpoint = tracing_service_->ConnectProducer(producer.get(), 0, name);
  producer->svc_endpoint_.reset(new NodeProducerEndpoint(std::move(endpoint), task_runner_));
  producers_.push_back(producer);
}

}
}
}
