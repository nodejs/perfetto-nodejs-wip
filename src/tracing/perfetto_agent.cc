#include "tracing/perfetto_agent.h"
#include "perfetto/tracing/data_source.h"
#include "perfetto/ext/tracing/core/data_source_descriptor.h"
#include "perfetto/ext/tracing/core/trace_config.h"
#include "perfetto/trace/chrome/chrome_trace_event.pbzero.h"

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
  }

  const uint8_t* GetCategoryGroupEnabled(const char* name) override {
    return category_manager_->GetCategoryGroupEnabled(name);
  }


 private:
  std::unique_ptr<CategoryManager> category_manager_;
};

// class NodePerfettoPlatform : public perfetto::Platform {
//   ThreadLocalObject* GetOrCreateThreadLocalObject() override {}

//   std::unique_ptr<base::TaskRunner> CreateTaskRunner(
//     const CreateTaskRunnerArgs& args) override {
    
//   }

//   std::string GetCurrentProcessName() override { return "node"; }
// };

PerfettoAgent::PerfettoAgent() {
  // Initialize the Perfetto backend.
  perfetto::TracingInitArgs tracing_init_args;
  tracing_init_args.backends = perfetto::BackendType::kInProcessBackend;
  perfetto::Tracing::Initialize(tracing_init_args);

  // Initialize a data source, as well as the TracingController instance that
  // will write to this data source.

  perfetto::DataSourceDescriptor descriptor;
  descriptor.set_name("node_trace_events");
  NodeDataSource::Register(descriptor);

  tracing_controller_.reset(new PerfettoTracingController());

  // Create a configuration for a tracing session, and start it.

  perfetto::TraceConfig config;
  config.add_buffers()->set_size_kb(4096);
  {
    auto c = config.add_data_sources();
    auto c2 = c->mutable_config();
    c2->set_name("node_trace_events");
  }
  config.set_write_into_file(true);
  uint64_t file_size_bytes = 4096;
  file_size_bytes <<= 10;
  config.set_max_file_size_bytes(file_size_bytes);
  config.set_file_write_period_ms(2000);
  // Note: Setup doesn't accept an FD. We need to start and write our own
  // loop to consume trace data.

  tracing_session_ = perfetto::Tracing::NewTrace(perfetto::BackendType::kInProcessBackend); 
  tracing_session_->Setup(config);
  tracing_session_->Start();
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

}
}
