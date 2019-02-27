#ifndef SRC_TRACING_FILE_WRITER_CONSUMER_H_
#define SRC_TRACING_FILE_WRITER_CONSUMER_H_

#include "tracing/base/node_tracing.h"
#include <set>

namespace node {
namespace tracing {

/**
 * Options used to construct a FileWriterConsumer.
 */
struct FileWriterConsumerOptions {
  /**
   * The log file pattern. The strings ${pid} and ${rotation} will be
   * substituted for Process ID and rotation number, respectively.
   */
  const char* log_file_pattern;
  /**
   * The buffer size in KB.
   * Note that this needs to be a multiple of the page size, currently 4KB.
   */
  uint32_t buffer_size_kb;
  /**
   * The minimum (?) file size before rotating to a new file.
   * TODO(kjin): Verify whether this is minimum or maximum.
   */
  uint32_t file_size_kb;
  /**
   * The amount of time that should elapse between file writes, in MS.
   */
  uint32_t file_write_period_ms;
};

/**
 * A Perfetto Consumer that doesn't consume traces directly, instead directing
 * producers to write to a file.
 */
class FileWriterConsumer : public base::NodeConsumer {
 public:
  /**
   * Constructs a FileWriterConsumer with the given options.
   */
  FileWriterConsumer(FileWriterConsumerOptions& options): options_(options) {}
  /**
   * Enables the consumer.
   * TODO(kjin): categories is currently ignored.
   */
  void Enable(const std::set<std::string>& categories);
  /**
   * Disables the consumer.
   * TODO(kjin): categories is currently ignored.
   */
  void Disable(const std::set<std::string>& categories);
  bool IsEnabled() { return enabled_; }
 private:
  void OnTracingDisabled() override {
    GetServiceEndpoint()->FreeBuffers();
    RotateFilenameAndEnable();
  }

  void RotateFilenameAndEnable();
  
  bool enabled_ = false;
  const FileWriterConsumerOptions options_;
  uint32_t file_num_ = 0;
};

}
}

#endif