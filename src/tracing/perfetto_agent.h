#ifndef SRC_TRACING_PERFETTO_AGENT_H_
#define SRC_TRACING_PERFETTO_AGENT_H_

#include "tracing/agent.h"
#include "perfetto/tracing/tracing.h"
#include "uv.h"

#define NODE_PERFETTO_DEBUG(s) printf("[PERFETTO] " s "\n")

namespace node {
namespace tracing {

namespace internal {

/**
 * The current tracing state.
 */
enum PerfettoAgentTracingState {
  STOPPED,
  STARTED,
  STOPPING
};

}

/**
 * Options used when tracing is started.
 */
struct TracingOptions {
  /**
   * The interval at which trace data should be passed to consumers.
   */
  uint32_t write_period_ms;
  /**
   * The amount of time to trace before automatically stopping, in milliseconds.
   * Specify 0 to trace forever.
   */
  uint32_t trace_duration_ms;
};

class PerfettoAgent;

/**
 * A base interface for any trace consumer.
 */
class TraceConsumer {
 public:
  virtual ~TraceConsumer() {}
  /**
   * Called periodically with trace data when added to a PerfettoAgent instance.
   */
  virtual void OnTrace(const std::vector<char>& raw) = 0;
  /**
   * Called to signal that OnTrace will start to be called.
   */
  virtual void OnTraceStarted() = 0;
  /**
   * Called to signal that OnTrace will not be called again.
   */
  virtual void OnTraceStopped() = 0;
};

/**
 * A base interface for objects that listen for events related to tracing.
 */
class TraceStateObserver {
 public:
  virtual ~TraceStateObserver() {}
  /**
   * Called when a PerfettoAgent is started.
   */
  virtual void OnTraceEnabled() = 0;
  /**
   * Called when a PerfettoAgent is stopped.
   */
  virtual void OnTraceDisabled() = 0;
};

/**
 * A handle that controls the lifecycle of a TraceConsumer that has been
 * added to a PerfettoAgent.
 */
class TraceConsumerHandle {
 public:
  inline TraceConsumerHandle() {}
  TraceConsumerHandle(TraceConsumerHandle&& other);
  TraceConsumerHandle& operator=(TraceConsumerHandle&& other);
  TraceConsumerHandle(const TraceConsumerHandle& other) = delete;
  TraceConsumerHandle& operator=(const TraceConsumerHandle& other) = delete;
  inline ~TraceConsumerHandle() { RemoveTraceConsumer(); }
  void RemoveTraceConsumer();
 private:
  inline TraceConsumerHandle(PerfettoAgent* agent, TraceConsumer* trace_writer)
    : agent_(agent), trace_writer_(trace_writer) {}

  PerfettoAgent* agent_ = nullptr;
  TraceConsumer* trace_writer_ = nullptr;
  friend class PerfettoAgent;
};

/**
 * The primary tracing interface. When at least one TraceConsumer objects are
 * added to this instance, tracing is enabled, and each TraceConsumer receives
 * resulting trace data periodically.
 */
class PerfettoAgent : public AgentBase {
 public:
  static PerfettoAgent& GetAgent();
  static v8::TracingController* DataSource();

  ~PerfettoAgent();

  /**
   * Initializes the Perfetto backend.
   */
  void Initialize();

  /**
   * Gets a reference to a TracingController implementation.
   */
  v8::TracingController* GetTracingController() override;

  /**
   * Adds an event with the given arguments which will be written periodically.
   */
  void AddMetadataEvent(
      const unsigned char* category_group_enabled,
      const char* name,
      int num_args,
      const char** arg_names,
      const unsigned char* arg_types,
      const uint64_t* arg_values,
      unsigned int flags) override;

  /**
   * Adds a TraceConsumer object to this instance, returning a
   * TraceConsumerHandle. When the returned value is destroyed, the added
   * TraceConsumer is removed from this instance.
   */
  TraceConsumerHandle AddTraceConsumer(std::unique_ptr<TraceConsumer> consumer);

  /**
   * Adds a TraceStateObserver object to this instance.
   * TODO(kjin): Add either a remove method, or have this function return
   * a handle.
   */
  void AddTraceStateObserver(TraceStateObserver* observer);

  void RemoveTraceStateObserver(TraceStateObserver* observer);

  /**
   * Start tracing with the given parameters.
   */
  void Start(TracingOptions options);

  /**
   * Explicitly stop tracing.
   */
  void Stop();
 private:
  /**
   * Constructs a new PerfettoAgent instance. This will initialize the Perfetto
   * tracing backend and any built-in data sources.
   */
  PerfettoAgent();

  /* The tracing state. */
  internal::PerfettoAgentTracingState tracing_state_ = internal::STOPPED;
  /* Tracing options. This is only valid when the tracing state is RUNNING. */
  TracingOptions options_;
  /* The underlying interface exposed by Perfetto for controlling tracing. */
  std::unique_ptr<perfetto::TracingSession> tracing_session_;
  /* All trace consumers added to this instance. */
  std::set<TraceConsumer*> consumers_;
  /* All trace state observers added to this instance. */
  std::set<TraceStateObserver*> observers_;

  /* The thread on which traces will be consumed. */
  uv_thread_t thread_;
  uv_loop_t tracing_loop_;
  /* The timer that controls periodic trace consumption. */
  uv_timer_t timer_;

  /* The V8 tracing controller. */
  std::unique_ptr<v8::TracingController> tracing_controller_;

  friend class TraceConsumerHandle;
};

}
}

#endif  // SRC_TRACING_PERFETTO_AGENT_H_
