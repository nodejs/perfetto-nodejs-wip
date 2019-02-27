#ifndef SRC_NODE_TRACING_H_
#define SRC_NODE_TRACING_H_

#include "tracing/base/node_tracing.h"

#include "tracing/file_writer_consumer.h"
#include "tracing/tracing_controller_producer.h"

namespace node {

namespace per_process {
  extern tracing::base::NodeTracing tracing;
} // namespace per_process

inline tracing::base::NodeTracing& GetNodeTracing() {
  return per_process::tracing;
}

} // namespace node

#endif
