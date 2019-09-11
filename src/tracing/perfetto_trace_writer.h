#ifndef SRC_TRACING_PERFETTO_TRACE_WRITER_H_
#define SRC_TRACING_PERFETTO_TRACE_WRITER_H_

#include "tracing/perfetto_agent.h"
#include "perfetto/trace/trace.pb.h"

#include <fstream>

using v8::platform::tracing::TraceObject;

namespace node {
namespace tracing {

/**
 * Prints useful data about the current trace data to the console.
 */
class StdoutTraceConsumer : public TraceConsumer {
  void OnTrace(const std::vector<char>& raw) override {
    // std::ofstream output;
    // output.open("output.proto", std::ios::app | std::ios::binary);
    // output.write(c, raw.size());
    // output.close();
    // delete[] c;
    // printf("Wrote %d bytes\n", raw.size());

    perfetto::protos::Trace trace;
    trace.ParseFromArray(raw.data(), raw.size());
    printf(">>> Consumed: %s --- packet size %d\n", trace.GetTypeName().c_str(), trace.packet_size());
    std::map<uint64_t, uint32_t> m;
    for (auto i = 0; i < trace.packet_size(); i++) {
      auto packet = trace.packet(i);
      auto chrome_events = packet.chrome_events();
      auto events = chrome_events.trace_events();
      for (auto itr = events.pointer_begin(); itr != events.pointer_end(); itr++) {
        m[(*itr)->process_id()]++;
      }
    }
    for (auto itr = m.begin(); itr != m.end(); itr++) {
      printf(">> %d %d\n", itr->first, itr->second);
    }
  }
  
  void OnTraceStarted() override {}
  void OnTraceStopped() override {}
};

/**
 * Writes Protobuf directly to a file.
 */
class FileTraceConsumer : public TraceConsumer {
  void OnTrace(const std::vector<char>& raw) override {
    std::ofstream output;
    output.open("output.proto", std::ios::app | std::ios::binary);
    output.write(raw.data(), raw.size());
    output.close();
    printf("[PERFETTO WRITER] Wrote %d bytes\n", raw.size());
  }
  
  void OnTraceStarted() override {}
  void OnTraceStopped() override {}
};

class BlockingTraceConsumer : public TraceConsumer {
  void OnTrace(const std::vector<char>& raw) override {
    static uint64_t a = 0;
    {
      perfetto::protos::Trace trace;
      trace.ParseFromArray(raw.data(), raw.size());
      a++;
      printf(">>> %d Blocking for two seconds: %d packets, %d bytes\n", a, trace.packet_size(), raw.size());
      sleep(2);
    }
    printf(">>> %d Blocking finished\n", a);
  }
  
  void OnTraceStarted() override {}
  void OnTraceStopped() override {}
};

/**
 * Wraps an existing AsyncTraceWriter.
 * Note: Once tracing is stopped, the AsyncTraceWriter is destroyed; in other
 * words an instance of this class can't be re-used.
 */
class LegacyTraceConsumer : public TraceConsumer {
 public:
  LegacyTraceConsumer(std::unique_ptr<AsyncTraceWriter> trace_writer)
      : trace_writer_(std::move(trace_writer)) {
    controller_ = std::make_unique<v8::platform::tracing::TracingController>();
    controller_->Initialize(nullptr);
    uv_loop_init(&tracing_loop_);
    trace_writer_->InitializeOnThread(&tracing_loop_);
    uv_thread_create(&thread_, [](void* arg) {
      LegacyTraceConsumer* consumer = static_cast<LegacyTraceConsumer*>(arg);
      uv_run(&consumer->tracing_loop_, UV_RUN_DEFAULT);
    }, this);
  }

  void OnTrace(const std::vector<char>& raw) override {
    TraceObject t;
    perfetto::protos::Trace trace;
    trace.ParseFromArray(raw.data(), raw.size());
    for (auto i = 0; i < trace.packet_size(); i++) {
      auto packet = trace.packet(i);
      auto chrome_events = packet.chrome_events();
      auto events = chrome_events.trace_events();
      for (auto itr = events.pointer_begin(); itr != events.pointer_end(); itr++) {
        auto event = *itr;
        // Consults global table.
        auto category_group_ptr =
            controller_->GetCategoryGroupEnabled(event->category_group_name().c_str());
        // auto args = event->args();
        // if (args.size() == 0) {
        t.Initialize(
          event->phase(),
          category_group_ptr,
          event->name().c_str(),
          event->scope().c_str(),
          event->id(),
          event->bind_id(),
          0, nullptr, nullptr, nullptr, nullptr,
          event->flags(), event->timestamp(), event->thread_timestamp()
        );
        // }
        trace_writer_->AppendTraceEvent(&t);
      }
    }
    trace_writer_->Flush(true);
  }

  void OnTraceStarted() override {}
  
  void OnTraceStopped() override {
    trace_writer_.reset();
  }
 private:
  std::unique_ptr<v8::platform::tracing::TracingController> controller_;
  std::unique_ptr<AsyncTraceWriter> trace_writer_;

  uv_thread_t thread_;
  uv_loop_t tracing_loop_;
};

}
}

#endif
