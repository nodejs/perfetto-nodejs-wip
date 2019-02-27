#include "tracing/tracing_controller_producer.h"
#include "tracing/trace_event.h"
#include "perfetto/trace/clock_snapshot.pbzero.h"
#include "uv.h"
#include <algorithm>
#include <thread>

namespace node {
namespace tracing {

class TimestampManager : protected v8::platform::tracing::TracingController {
 public:
  ~TimestampManager() {
    Initialize(nullptr);
  }
  int64_t CurrentTimestampMicroseconds() {
    return uv_hrtime() / 1000;
  }
  int64_t CurrentCpuTimestampMicroseconds() {
    return TracingController::CurrentCpuTimestampMicroseconds();
  }
} g_timestamp_manager;

const uint64_t kNoHandle = 0;

const uint8_t* PerfettoTracingController::GetCategoryGroupEnabled(const char* name) {
  return category_manager_->GetCategoryGroupEnabled(name);
}

uint64_t PerfettoTracingController::AddTraceEventWithTimestamp(
    char phase, const uint8_t* category_enabled_flag, const char* name,
    const char* scope, uint64_t id, uint64_t bind_id, int32_t num_args,
    const char** arg_names, const uint8_t* arg_types,
    const uint64_t* arg_values,
    std::unique_ptr<v8::ConvertableToTraceFormat>* arg_convertables,
    unsigned int flags, int64_t timestamp) {
  // Hack to prevent flushes from happen at every trace event.
  // Every flush posts a new task to the task runner, so this might be needed
  // to prevent infinite loops.
  if (std::strcmp(name, "CheckImmediate") == 0) {
    return kNoHandle;
  }
  if (!this->enabled_) {
    return kNoHandle;
  }
  auto trace_writer = producer_->GetTLSTraceWriter();
  static uint64_t handle = kNoHandle + 1;
  static std::hash<std::thread::id> hasher;
  static std::mutex writer_lock_;
  std::lock_guard<std::mutex> scoped_lock(writer_lock_);
  {
    auto trace_packet = trace_writer->NewTracePacket();
    auto trace_event_bundle = trace_packet->set_chrome_events();
    auto trace_event = trace_event_bundle->add_trace_events();
    trace_event->set_process_id(uv_os_getpid());
    // Imperfect way to get a thread ID in lieu of V8's built-in mechanism.
    trace_event->set_thread_id(hasher(std::this_thread::get_id()) % 65536);
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
  }
  trace_writer->Flush();
  return handle++;
}

void PerfettoTracingController::AddTraceStateObserver(TraceStateObserver* observer) {
  if (observers_.find(observer) != observers_.end()) {
    return;
  }
  observers_.insert(observer);
  if (enabled_) {
    observer->OnTraceEnabled();
  }
}

void PerfettoTracingController::RemoveTraceStateObserver(TraceStateObserver* observer) {
  observers_.erase(observer);
}

void TracingControllerProducer::Agent::AddMetadataEvent(
    const unsigned char* category_group_enabled,
    const char* name,
    int num_args,
    const char** arg_names,
    const unsigned char* arg_types,
    const uint64_t* arg_values,
    unsigned int flags) {
  std::unique_ptr<v8::ConvertableToTraceFormat> arg_convertibles[2];
  if (num_args > 0 && arg_types[0] == TRACE_VALUE_TYPE_CONVERTABLE) {
    arg_convertibles[0].reset(reinterpret_cast<v8::ConvertableToTraceFormat*>(
        static_cast<intptr_t>(arg_values[0])));
  }
  if (num_args > 1 && arg_types[1] == TRACE_VALUE_TYPE_CONVERTABLE) {
    arg_convertibles[1].reset(reinterpret_cast<v8::ConvertableToTraceFormat*>(
        static_cast<intptr_t>(arg_values[1])));
  }
  std::unique_ptr<TraceObject> trace_event(new TraceObject);
  trace_event->Initialize(
    TRACE_EVENT_PHASE_METADATA, category_group_enabled, name,
    node::tracing::kGlobalScope,  // scope
    node::tracing::kNoId,         // id
    node::tracing::kNoId,         // bind_id
    num_args, arg_names, arg_types, arg_values, arg_convertibles,
    TRACE_EVENT_FLAG_NONE,
    g_timestamp_manager.CurrentTimestampMicroseconds(),
    g_timestamp_manager.CurrentCpuTimestampMicroseconds());
  Mutex::ScopedLock lock(events_mutex_);
  events_.push_back(std::move(trace_event));
}

void TracingControllerProducer::WriteMetadataEvents() {
  auto trace_writer = GetTLSTraceWriter();
  {
    auto trace_packet = trace_writer->NewTracePacket();
    auto trace_event_bundle = trace_packet->set_chrome_events();
    Mutex::ScopedLock lock(agent_->events_mutex_);
    auto& events = agent_->events_;
    for (std::list<std::unique_ptr<TraceObject>>::const_iterator itr = events.begin();
            itr != events.end(); itr++) {
      auto event = itr->get();
      auto trace_event = trace_event_bundle->add_trace_events();
      trace_event->set_process_id(event->pid());
      trace_event->set_thread_id(event->tid());
      trace_event->set_phase(event->phase());
      trace_event->set_category_group_name(
          category_manager_->GetCategoryGroupName(event->category_enabled_flag()));
      trace_event->set_name(event->name());
      trace_event->set_scope(event->scope());
      trace_event->set_id(event->id());
      trace_event->set_bind_id(event->bind_id());
      trace_event->set_timestamp(event->ts());
      trace_event->set_thread_timestamp(event->tts());
      // Maybe these two need to be switched
      trace_event->set_duration(event->duration());
      trace_event->set_thread_duration(event->cpu_duration());
    }
  }
  trace_writer->Flush();
}

TracingControllerProducer::TracingControllerProducer()
  : trace_controller_(new PerfettoTracingController(this)),
    agent_(new TracingControllerProducer::Agent(trace_controller_)),
    category_manager_(new CategoryManager()) {
  trace_controller_->category_manager_ = category_manager_.get();
}

void TracingControllerProducer::OnConnect() {
  perfetto::DataSourceDescriptor ds_desc;
  ds_desc.set_name("trace_events");
  GetServiceEndpoint()->RegisterDataSource(ds_desc);
}

void TracingControllerProducer::SetupDataSource(perfetto::DataSourceInstanceID id,
                      const perfetto::DataSourceConfig& cfg) {
  data_source_id_ = id;
}

void TracingControllerProducer::StartDataSource(perfetto::DataSourceInstanceID id,
                      const perfetto::DataSourceConfig& cfg) {
  if (data_source_id_ != id) {
    return;
  }
  {
    auto observers = trace_controller_->observers_;
    for (auto itr = observers.begin(); itr != observers.end(); itr++) {
      (*itr)->OnTraceEnabled();
    }
  }
  target_buffer_ = cfg.target_buffer();
  trace_controller_->enabled_ = true;
  // TODO(kjin): Don't hardcode these
  std::list<const char*> groups = { "node", "node.async_hooks", "v8" };
  category_manager_->UpdateCategoryGroups(groups);
  WriteMetadataEvents();
}

void TracingControllerProducer::StopDataSource(perfetto::DataSourceInstanceID id) {
  if (data_source_id_ != id) {
    return;
  }
  Cleanup();
}

perfetto::TraceWriter* TracingControllerProducer::GetTLSTraceWriter() {
  if (!trace_writer_) {
    trace_writer_ = GetServiceEndpoint()->CreateTraceWriter(target_buffer_);
  }
  return trace_writer_.get();
}

void TracingControllerProducer::Cleanup() {
  if (trace_controller_->enabled_) {
    trace_controller_->enabled_ = false;
    {
      std::list<const char*> groups = {};
      category_manager_->UpdateCategoryGroups(groups);
      auto observers = trace_controller_->observers_;
      for (auto itr = observers.begin(); itr != observers.end(); itr++) {
        (*itr)->OnTraceDisabled();
      }
    }
    trace_writer_.reset(nullptr);
  }
}

}
}
