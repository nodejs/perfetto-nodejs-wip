#ifndef SRC_NODE_V8_PLATFORM_INL_H_
#define SRC_NODE_V8_PLATFORM_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <memory>

#include "env-inl.h"
#include "node.h"
#include "node_metadata.h"
#include "node_options.h"
#include "tracing/node_trace_writer.h"
#include "tracing/trace_event.h"
#include "tracing/traced_value.h"

#include "tracing/perfetto_agent.h"
#include "tracing/perfetto_trace_writer.h"

namespace node {

// Ensures that __metadata trace events are only emitted
// when tracing is enabled.
class NodeTraceStateObserver : public tracing::TraceStateObserver {
 public:
  inline void OnTraceEnabled() override {
    char name_buffer[512];
    if (uv_get_process_title(name_buffer, sizeof(name_buffer)) == 0) {
      // Only emit the metadata event if the title can be retrieved
      // successfully. Ignore it otherwise.
      TRACE_EVENT_METADATA1(
          "__metadata", "process_name", "name", TRACE_STR_COPY(name_buffer));
    }
    TRACE_EVENT_METADATA1("__metadata",
                          "version",
                          "node",
                          per_process::metadata.versions.node.c_str());
    TRACE_EVENT_METADATA1(
        "__metadata", "thread_name", "name", "JavaScriptMainThread");

    auto trace_process = tracing::TracedValue::Create();
    trace_process->BeginDictionary("versions");

#define V(key)                                                                 \
  trace_process->SetString(#key, per_process::metadata.versions.key.c_str());

    NODE_VERSIONS_KEYS(V)
#undef V

    trace_process->EndDictionary();

    trace_process->SetString("arch", per_process::metadata.arch.c_str());
    trace_process->SetString("platform",
                             per_process::metadata.platform.c_str());

    trace_process->BeginDictionary("release");
    trace_process->SetString("name",
                             per_process::metadata.release.name.c_str());
#if NODE_VERSION_IS_LTS
    trace_process->SetString("lts", per_process::metadata.release.lts.c_str());
#endif
    trace_process->EndDictionary();
    TRACE_EVENT_METADATA1(
        "__metadata", "node", "process", std::move(trace_process));

    // This only runs the first time tracing is enabled
    tracing::PerfettoAgent::GetAgent().RemoveTraceStateObserver(this);
  }

  inline void OnTraceDisabled() override {
    // Do nothing here. This should never be called because the
    // observer removes itself when OnTraceEnabled() is called.
    UNREACHABLE();
  }

  ~NodeTraceStateObserver() override = default;
};

struct V8Platform {
#if NODE_USE_V8_PLATFORM
  inline void Initialize(int thread_pool_size) {
    // [old tracing system]
    tracing_agent_ = std::make_unique<tracing::Agent>();
    tracing::PerfettoAgent::GetAgent().Initialize();
    trace_state_observer_ = std::make_unique<NodeTraceStateObserver>();
    tracing::PerfettoAgent::GetAgent().AddTraceStateObserver(trace_state_observer_.get());
    // [old tracing system]
    tracing_file_writer_ = tracing_agent_->DefaultHandle();
    // Only start the tracing agent if we enabled any tracing categories.
    if (!per_process::cli_options->trace_event_categories.empty()) {
      StartTracingAgent();
    }
    // Tracing must be initialized before platform threads are created.
    platform_ = new NodePlatform(thread_pool_size,
        tracing::PerfettoAgent::GetAgent().GetTracingController());
    v8::V8::InitializePlatform(platform_);
  }

  inline void Dispose() {
    StopTracingAgent();
    platform_->Shutdown();
    delete platform_;
    platform_ = nullptr;
    // [old tracing system]
    // Destroy tracing after the platform (and platform threads) have been
    // stopped.
    tracing_agent_.reset(nullptr);
    trace_state_observer_.reset(nullptr);
  }

  inline void DrainVMTasks(v8::Isolate* isolate) {
    platform_->DrainTasks(isolate);
  }

  inline void CancelVMTasks(v8::Isolate* isolate) {
    platform_->CancelPendingDelayedTasks(isolate);
  }

  inline void StartTracingAgent() {
    // Attach a new NodeTraceWriter only if this function hasn't been called
    // before.
    // [old tracing system]
    if (tracing_file_writer_.IsDefaultHandle()) {
      std::vector<std::string> categories =
          SplitString(per_process::cli_options->trace_event_categories, ',');

      tracing_file_writer_ = tracing_agent_->AddClient(
          std::set<std::string>(std::make_move_iterator(categories.begin()),
                                std::make_move_iterator(categories.end())),
          std::unique_ptr<tracing::AsyncTraceWriter>(
              new tracing::NodeTraceWriter(
                  per_process::cli_options->trace_event_file_pattern)),
          tracing::Agent::kUseDefaultCategories);
    }
    tracing_perfetto_writer_ = tracing::PerfettoAgent::GetAgent().AddTraceConsumer(std::make_unique<tracing::BlockingTraceConsumer>());
    // tracing_perfetto_writer_2_ = tracing::PerfettoAgent::GetAgent().AddTraceConsumer(std::make_unique<tracing::StdoutTraceConsumer>());
    // tracing_perfetto_writer_2_ = tracing::PerfettoAgent::GetAgent().AddTraceConsumer(
    //     std::make_unique<tracing::LegacyTraceConsumer>(
    //         std::unique_ptr<tracing::AsyncTraceWriter>(
    //             new tracing::NodeTraceWriter(per_process::cli_options->trace_event_file_pattern))));
    tracing::TracingOptions trace_options;
    trace_options.write_period_ms = 1000;
    trace_options.trace_duration_ms = 60000;
    tracing::PerfettoAgent::GetAgent().Start(trace_options);
  }

  inline void StopTracingAgent() {
    // [old tracing system]
    tracing_file_writer_.reset();
    tracing::PerfettoAgent::GetAgent().Stop();
    // TODO(kjin): Should this be removed asynchronously?
    tracing_perfetto_writer_.RemoveTraceConsumer();
  }

  // [old tracing system]
  inline tracing::AgentWriterHandle* GetTracingAgentWriter() {
    return &tracing_file_writer_;
  }

  inline NodePlatform* Platform() { return platform_; }

  std::unique_ptr<NodeTraceStateObserver> trace_state_observer_;
  // [old tracing system]
  std::unique_ptr<tracing::Agent> tracing_agent_;
  // [old tracing system]
  tracing::AgentWriterHandle tracing_file_writer_;
  tracing::TraceConsumerHandle tracing_perfetto_writer_;
  tracing::TraceConsumerHandle tracing_perfetto_writer_2_;
  NodePlatform* platform_;
#else   // !NODE_USE_V8_PLATFORM
  inline void Initialize(int thread_pool_size) {}
  inline void Dispose() {}
  inline void DrainVMTasks(v8::Isolate* isolate) {}
  inline void CancelVMTasks(v8::Isolate* isolate) {}
  inline void StartTracingAgent() {
    if (!per_process::cli_options->trace_event_categories.empty()) {
      fprintf(stderr,
              "Node compiled with NODE_USE_V8_PLATFORM=0, "
              "so event tracing is not available.\n");
    }
  }
  inline void StopTracingAgent() {}

  inline tracing::AgentWriterHandle* GetTracingAgentWriter() { return nullptr; }

  inline NodePlatform* Platform() { return nullptr; }
#endif  // !NODE_USE_V8_PLATFORM
};

namespace per_process {
extern struct V8Platform v8_platform;
}

inline void StartTracingAgent() {
  return per_process::v8_platform.StartTracingAgent();
}

inline tracing::AgentWriterHandle* GetTracingAgentWriter() {
  return per_process::v8_platform.GetTracingAgentWriter();
}

inline void DisposePlatform() {
  per_process::v8_platform.Dispose();
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_V8_PLATFORM_INL_H_
