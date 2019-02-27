#ifndef SRC_TRACING_BASE_NODE_TASK_RUNNER_H_
#define SRC_TRACING_BASE_NODE_TASK_RUNNER_H_

#include "perfetto/base/task_runner.h"
#include "debug_utils.h"
#include "uv.h"
#include "v8.h"

#include <list>
#include <mutex>

namespace node {
namespace tracing {
namespace base {

class NodeTaskRunner : public perfetto::base::TaskRunner {
 public:
  NodeTaskRunner() {}
  ~NodeTaskRunner() {}

  void PostTask(std::function<void()> task) override;
  void PostDelayedTask(std::function<void()> task, uint32_t delay_ms) override;

  void AddFileDescriptorWatch(int fd, std::function<void()>) override {}
  void RemoveFileDescriptorWatch(int fd) override {}
  bool RunsTasksOnCurrentThread() const override { return true; }
  virtual void Start();
 private:
  v8::Isolate* isolate_;
};

/**
 * This class is a super-hack to circumvent two things:
 * - Can't start the task runner immediately because isolate isn't set up
 * - Don't want to keep the process alive with period write requests
 */
class DelayedNodeTaskRunner : public NodeTaskRunner {
 public:
  void PostTask(std::function<void()> task) override;
  void PostDelayedTask(std::function<void()> task, uint32_t delay_ms) override;
  void Start() override;
 private:
  std::list<std::pair<std::function<void()>, uint32_t>> delayed_args_;
  bool started_ = false;
};

}
}
}

#endif
