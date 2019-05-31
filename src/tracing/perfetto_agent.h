#ifndef SRC_TRACING_PERFETTO_AGENT_H_
#define SRC_TRACING_PERFETTO_AGENT_H_

#include "tracing/agent.h"
#include "perfetto/tracing/tracing.h"

namespace node {
namespace tracing {

class PerfettoAgent : public AgentBase {
 public:
  PerfettoAgent();
  v8::TracingController* GetTracingController() override;
  void AddMetadataEvent(
      const unsigned char* category_group_enabled,
      const char* name,
      int num_args,
      const char** arg_names,
      const unsigned char* arg_types,
      const uint64_t* arg_values,
      unsigned int flags) override;
 private:
  std::unique_ptr<v8::TracingController> tracing_controller_;
  std::unique_ptr<perfetto::TracingSession> tracing_session_;
};

}
}

#endif  // SRC_TRACING_PERFETTO_AGENT_H_
