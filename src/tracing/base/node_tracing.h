#ifndef SRC_TRACING_BASE_NODE_TRACING_H_
#define SRC_TRACING_BASE_NODE_TRACING_H_

#include "perfetto/tracing/core/tracing_service.h"
#include "perfetto/tracing/core/consumer.h"
#include "perfetto/tracing/core/producer.h"
#include "tracing/base/node_task_runner.h"
#include "tracing/base/node_consumer_endpoint.h"
#include "tracing/base/node_producer_endpoint.h"

#include <list>

using perfetto::TracingService;

namespace node {
namespace tracing {
namespace base {

class NodeTracing;

/**
 * A skeleton implementation of a Perfetto Consumer that manages its own
 * service endpoint instance.
 * Concrete implementations should subclass this class.
 */
class NodeConsumer : public perfetto::Consumer {
 protected:
  /**
   * Gets the service endpoint, or null if this instance isn't yet connected
   * to a Service.
   */
  inline perfetto::TracingService::ConsumerEndpoint* GetServiceEndpoint() { return svc_endpoint_.get(); }

  void OnConnect() override {}
  void OnDisconnect() override {}
  void OnTracingDisabled() override {}
  void OnTraceData(std::vector<perfetto::TracePacket> packets,
                   bool has_more) override {}
  void OnDetach(bool success) override {}
  void OnAttach(bool success, const perfetto::TraceConfig&) override {}
  void OnTraceStats(bool success, const perfetto::TraceStats&) override {}
 private:
  std::unique_ptr<NodeConsumerEndpoint> svc_endpoint_;
  friend class NodeTracing;
};

/**
 * A skeleton implementation of a Perfetto Producer that manages its own
 * service endpoint instance.
 * Concrete implementations should subclass this class.
 */
class NodeProducer : public perfetto::Producer {
 protected:
  /**
   * Gets the service endpoint, or null if this instance isn't yet connected
   * to a Service.
   */
  inline perfetto::TracingService::ProducerEndpoint* GetServiceEndpoint() { return svc_endpoint_.get(); }

  void OnConnect() override {}
  void OnDisconnect() override {}
  void OnTracingSetup() override {}
  void SetupDataSource(perfetto::DataSourceInstanceID,
                       const perfetto::DataSourceConfig&) override {}
  void StartDataSource(perfetto::DataSourceInstanceID,
                       const perfetto::DataSourceConfig& cfg) override {}
  void StopDataSource(perfetto::DataSourceInstanceID) override {}
  void Flush(perfetto::FlushRequestID,
             const perfetto::DataSourceInstanceID*, size_t) override {}
 private:
  std::unique_ptr<NodeProducerEndpoint> svc_endpoint_;
  friend class NodeTracing;
};

/**
 * A wrapper around Perfetto's TracingService implementation.
 * Only clients that subclass NodeConsumer or NodeProducer can be connected.
 * In addition to using a baked-in task runner and shared memory implementation,
 * this class safely shuts down the underlying TracingService if there are
 * clients that are still connected.
 */
class NodeTracing {
 public:
  NodeTracing();
  ~NodeTracing();

  /**
   * Initialize the tracing service.
   */
  void Initialize();

  /**
   * Connect a NodeConsumer instance.
   */
  void ConnectConsumer(std::shared_ptr<NodeConsumer> consumer);

  /**
   * Connect a NodeProducer instance under a given name.
   */
  void ConnectProducer(std::shared_ptr<NodeProducer> producer, std::string name);
 private:
  // References to attached consumers. Needed to forcibly disconnect consumers
  // if the tracing service goes down first.
  std::list<std::weak_ptr<NodeConsumer>> consumers_;
  // References to attached producers. Needed to forcibly disconnect producers
  // if the tracing service goes down first.
  std::list<std::weak_ptr<NodeProducer>> producers_;

  // A task runner with which the Perfetto service will post tasks. It runs all
  // tasks on the foreground thread.
  std::shared_ptr<NodeTaskRunner> task_runner_;
  // The Perfetto tracing service.
  std::unique_ptr<perfetto::TracingService> tracing_service_;
};

}
}
}

#endif
