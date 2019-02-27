#include "tracing/file_writer_consumer.h"
#include "perfetto/base/scoped_file.h"
#include "perfetto/tracing/core/trace_config.h"

namespace node {
namespace tracing {

void FileWriterConsumer::Enable(const std::set<std::string>& categories) {
  RotateFilenameAndEnable();
  enabled_ = true;
}

void FileWriterConsumer::Disable(const std::set<std::string>& categories) {
  GetServiceEndpoint()->DisableTracing();
  enabled_ = false;
}

// From NodeTraceWriter
void ReplaceSubstring(std::string* target,
                      const std::string& search,
                      const std::string& insert) {
  size_t pos = target->find(search);
  for (; pos != std::string::npos; pos = target->find(search, pos)) {
    target->replace(pos, search.size(), insert);
    pos += insert.size();
  }
}

void FileWriterConsumer::RotateFilenameAndEnable() {
  // Rotate the name.
  std::string log_file_pattern(options_.log_file_pattern);
  ReplaceSubstring(&log_file_pattern,
      "${pid}", std::to_string(uv_os_getpid()));
  ReplaceSubstring(&log_file_pattern,
      "${rotation}", std::to_string(++file_num_));
  // For clarity:
  // - "options" was used to set up this _consumer_
  // - "config" will be sent to enable the _producer_
  perfetto::TraceConfig config;
  config.add_buffers()->set_size_kb(options_.buffer_size_kb);
  {
    auto c = config.add_data_sources();
    auto c2 = c->mutable_config();
    c2->set_name("trace_events");
  }
  config.set_write_into_file(true);
  uint64_t file_size_bytes = options_.file_size_kb;
  file_size_bytes <<= 10;
  config.set_max_file_size_bytes(file_size_bytes);
  config.set_disable_clock_snapshotting(true);
  config.set_file_write_period_ms(options_.file_write_period_ms);
  
  perfetto::base::ScopedFile fd(
      open(log_file_pattern.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644));
  GetServiceEndpoint()->EnableTracing(config, std::move(fd));
}

}
}