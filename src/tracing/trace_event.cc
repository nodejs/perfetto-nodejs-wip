#include "tracing/trace_event.h"

namespace node {
namespace tracing {

AgentBase* g_agent;

void TraceEventHelper::SetAgent(AgentBase* agent) {
  g_agent = agent;
}

AgentBase* TraceEventHelper::GetAgent() {
  return g_agent;
}

v8::TracingController* TraceEventHelper::GetTracingController() {
  return g_agent->GetTracingController();
}

}  // namespace tracing
}  // namespace node
