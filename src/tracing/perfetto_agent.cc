#include "tracing/perfetto_agent.h"
#include "perfetto/tracing/data_source.h"
#include "perfetto/tracing/core/data_source_config.h"
#include "perfetto/tracing/core/data_source_descriptor.h"
#include "perfetto/tracing/core/trace_config.h"
#include "perfetto/trace/chrome/chrome_trace_event.pbzero.h"
#include "perfetto/trace/trace.pb.h"

/**
 * Data source for traces coming from Node.
 * Macros should eventually be implemented with this.
 */
class NodeDataSource : public perfetto::DataSource<NodeDataSource> {
};
PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS(NodeDataSource);

namespace node {
namespace tracing {

// V8 probably does a much better job at handling categories.
// Just use that for now.
class CategoryManager {
 public:
  CategoryManager() {
    controller_.Initialize(nullptr);
  }

  const uint8_t* GetCategoryGroupEnabled(const char* category_group) {
    auto result = controller_.GetCategoryGroupEnabled(category_group);
    return result;
  }

  const char* GetCategoryGroupName(const uint8_t* category_group_flags) {
    return controller_.GetCategoryGroupName(category_group_flags);
  }

  void UpdateCategoryGroups(std::list<const char*> categories) {
    auto config = v8::platform::tracing::TraceConfig::CreateDefaultTraceConfig();
    for (auto itr = categories.begin(); itr != categories.end(); itr++) {
      config->AddIncludedCategory(*itr);
    };
    controller_.StartTracing(config);
  }
 private:
  v8::platform::tracing::TracingController controller_;
};

class V8PerfettoTracingController : public v8::platform::tracing::TracingController {
 public:
  V8PerfettoTracingController() : v8::platform::tracing::TracingController() {}

  int64_t CurrentTimestampMicroseconds() override {
    return uv_hrtime() / 1000;
  }

  int64_t CurrentCpuTimestampMicroseconds() override {
    return DefaultTracingController::CurrentCpuTimestampMicroseconds();
  }
};

class PerfettoTracingController : public v8::TracingController {
 public:
  PerfettoTracingController(): category_manager_(new CategoryManager()) {
    category_manager_->UpdateCategoryGroups({ "node", "v8", "v8.async_hooks" });
  }

  uint64_t AddTraceEvent(
      char phase, const uint8_t* category_enabled_flag, const char* name,
      const char* scope, uint64_t id, uint64_t bind_id, int32_t num_args,
      const char** arg_names, const uint8_t* arg_types,
      const uint64_t* arg_values,
      std::unique_ptr<v8::ConvertableToTraceFormat>* arg_convertables,
      unsigned int flags) override {
    int64_t timestamp = uv_hrtime() / 1000;
    return AddTraceEventWithTimestamp(
      phase, category_enabled_flag, name, scope, id, bind_id, num_args,
      arg_names, arg_types, arg_values, std::move(arg_convertables), flags,
      timestamp);
  }

  uint64_t AddTraceEventWithTimestamp(
      char phase, const uint8_t* category_enabled_flag, const char* name,
      const char* scope, uint64_t id, uint64_t bind_id, int32_t num_args,
      const char** arg_names, const uint8_t* arg_types,
      const uint64_t* arg_values,
      std::unique_ptr<v8::ConvertableToTraceFormat>* arg_convertables,
      unsigned int flags, int64_t timestamp) override {
    NodeDataSource::Trace([=](NodeDataSource::TraceContext ctx) {
      auto trace_packet = ctx.NewTracePacket();
      auto trace_event_bundle = trace_packet->set_chrome_events();
      auto trace_event = trace_event_bundle->add_trace_events();
      trace_event->set_process_id(uv_os_getpid());
      trace_event->set_thread_id(0); // TODO
      trace_event->set_phase(phase);
      trace_event->set_category_group_name(
          category_manager_->GetCategoryGroupName(category_enabled_flag));
      trace_event->set_name(name);
      if (scope != nullptr) {
        trace_event->set_scope(scope);
      }
      trace_event->set_id(id);
      trace_event->set_bind_id(bind_id);
      trace_event->set_timestamp(timestamp);
      trace_event->set_thread_timestamp(timestamp);
      trace_event->set_duration(0);
      trace_event->set_thread_duration(0);
      trace_event->Finalize();
    });
    return 0;
  }

  const uint8_t* GetCategoryGroupEnabled(const char* name) override {
    return category_manager_->GetCategoryGroupEnabled(name);
  }

 private:
  std::unique_ptr<CategoryManager> category_manager_;
};

TraceConsumerHandle& TraceConsumerHandle::operator=(TraceConsumerHandle&& other) {
  RemoveTraceConsumer();
  agent_ = other.agent_;
  trace_writer_ = other.trace_writer_;
  other.agent_ = nullptr;
  other.trace_writer_ = nullptr;
  return *this;
}

TraceConsumerHandle::TraceConsumerHandle(TraceConsumerHandle&& other) {
  *this = std::move(other);
}

void TraceConsumerHandle::RemoveTraceConsumer() {
  if (agent_) {
    bool erased = !!agent_->consumers_.erase(trace_writer_);
    if (erased && agent_->consumers_.size() == 0) {
      agent_->Stop();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// PERFETTO ///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

PerfettoAgent::PerfettoAgent() {}

void PerfettoAgent::Initialize() {
  // Initialize the Perfetto backend.
  perfetto::TracingInitArgs tracing_init_args;
  tracing_init_args.backends = perfetto::BackendType::kInProcessBackend;
  perfetto::Tracing::Initialize(tracing_init_args);

  // Initialize a Perfetto data source.
  perfetto::DataSourceDescriptor descriptor;
  descriptor.set_name("node.trace_events");
  NodeDataSource::Register(descriptor);

  // Create the V8 tracing controller, which also initializes V8's data source.
  auto tracing_controller = new V8PerfettoTracingController();
  auto config = v8::platform::tracing::TraceConfig::CreateDefaultTraceConfig();
  config->AddIncludedCategory("node");
  config->AddIncludedCategory("node.async_hooks");
  tracing_controller->Initialize(nullptr);
  tracing_controller->StartTracing(config);
  tracing_controller_.reset(tracing_controller);

  // Create a new tracing session.
  tracing_session_ = perfetto::Tracing::NewTrace(perfetto::BackendType::kInProcessBackend);

  // Create UV primitives.
  uv_loop_init(&tracing_loop_);
  uv_timer_init(&tracing_loop_, &timer_);
  timer_.data = this;

  Mutex::ScopedLock lock(tracing_state_mutex_);
  tracing_state_ = internal::STOPPED;
}

PerfettoAgent::~PerfettoAgent() {
  // TODO
}

v8::TracingController* PerfettoAgent::GetTracingController() {
  return tracing_controller_.get();
}

void PerfettoAgent::AddMetadataEvent(
    const unsigned char* category_group_enabled,
    const char* name,
    int num_args,
    const char** arg_names,
    const unsigned char* arg_types,
    const uint64_t* arg_values,
    unsigned int flags) {
      // Not implemented yet.
    }

TraceConsumerHandle PerfettoAgent::AddTraceConsumer(std::unique_ptr<TraceConsumer> consumer) {
  auto trace_consumer = consumer.release();
  consumers_.insert(trace_consumer);
  if (tracing_state_ == internal::STARTED) {
    trace_consumer->OnTraceStarted();
  }
  NODE_PERFETTO_DEBUG("Trace Writer Added");
  return TraceConsumerHandle(this, trace_consumer);
}

void PerfettoAgent::AddTraceStateObserver(TraceStateObserver* observer) {
  observers_.insert(observer);
}

void PerfettoAgent::RemoveTraceStateObserver(TraceStateObserver* observer) {
  observers_.erase(observer);
}

void PerfettoAgent::Start(TracingOptions options) {
  {
    Mutex::ScopedLock lock(tracing_state_mutex_);
    while (tracing_state_ == internal::STOPPING) {
      tracing_state_stop_pending_cond_.Wait(lock);
    }
    if (tracing_state_ == internal::STARTED) return;
    tracing_state_ = internal::STARTED;
  }

  // Using options.trace_duration_ms, set up and start the tracing session.
  perfetto::TraceConfig config;
  if (options.trace_duration_ms != 0) {
    config.set_duration_ms(options.trace_duration_ms);
  }
  config.set_write_into_file(false);
  config.add_buffers()->set_size_kb(2048);
  {
    auto c = config.add_data_sources();
    auto c2 = c->mutable_config();
    c2->set_name("v8.trace_events");
  }
  tracing_session_->Setup(config);
  tracing_session_->SetOnStopCallback([=]() {
    Mutex::ScopedLock lock(tracing_state_mutex_);
    tracing_state_ = internal::STOPPING;
  });
  tracing_session_->Start();

  // Define a timer to consume traces that runs every options.write_period_ms
  // milliseconds.
  uv_timer_start(&timer_, [](uv_timer_t* handle) {
    PerfettoAgent* agent = static_cast<PerfettoAgent*>(handle->data);

    // Assert that when the tracing state is STOPPED, the timer is stopped as
    // well.
    DCHECK(agent->tracing_state_ != internal::STOPPED);

    // Read traces and call each consumer with trace data.
    std::vector<char> trace_data = agent->tracing_session_->ReadTraceBlocking();
    for (auto itr = agent->consumers_.begin(); itr != agent->consumers_.end(); itr++) {
      auto trace_writer = *itr;
      trace_writer->OnTrace(trace_data);
    }

    // If a stop signal was received, stop the timer and inform each consumer
    // that tracing has stopped.
    Mutex::ScopedLock lock(agent->tracing_state_mutex_);
    if (agent->tracing_state_ == internal::STOPPING) {
      agent->tracing_state_stop_pending_cond_.Broadcast(lock);
      agent->tracing_state_ = internal::STOPPED;
      uv_timer_stop(handle);
      Mutex::ScopedUnlock unlock(lock);
      NODE_PERFETTO_DEBUG("Tracing Stopped");
      for (auto itr = agent->observers_.begin(); false; itr++) {
        (*itr)->OnTraceDisabled();
      }
      for (auto itr = agent->consumers_.begin(); itr != agent->consumers_.end(); itr++) {
        (*itr)->OnTraceStopped();
      }
    }
  }, 0, options.write_period_ms);
  
  // Run this timer on a new thread.
  uv_thread_create(&thread_, [](void* arg) {
    PerfettoAgent* agent = static_cast<PerfettoAgent*>(arg);
    uv_run(&agent->tracing_loop_, UV_RUN_DEFAULT);
  }, this);

  NODE_PERFETTO_DEBUG("Tracing Started");

  // TODO(kjin): Investigate why this crashes if the continue condition isn't
  // just "false".
  for (auto itr = observers_.begin(); false; itr++) {
    (*itr)->OnTraceEnabled();
  }
  // Inform each consumer that tracing has started.
  for (auto itr = consumers_.begin(); itr != consumers_.end(); itr++) {
    (*itr)->OnTraceStarted();
  }
}

void PerfettoAgent::Stop() {
  {
    Mutex::ScopedLock lock(tracing_state_mutex_);
    if (tracing_state_ != internal::STARTED) return;
    tracing_state_ = internal::STOPPING;
  }

  NODE_PERFETTO_DEBUG("Tracing Stopping");
  tracing_session_->Stop();
}

PerfettoAgent& PerfettoAgent::GetAgent() {
  static PerfettoAgent g_agent_;
  return g_agent_;
}

v8::TracingController* PerfettoAgent::DataSource() {
  return GetAgent().GetTracingController();
}

}
}
