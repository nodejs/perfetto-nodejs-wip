# Perfetto for Node.js

This repository is a work-in-progress for integrating Perfetto into Node.js.

## What is Perfetto and how does it help Node.js?

Perfetto is an open-source tracing system, currently used in Chrome and Android (with V8 integration coming soon). In Chrome and V8, it subsumes the trace events subsystem.

Integrating Perfetto in Node means replacing our current trace events implementation with a more robust, shared implementation used across many open-source projects, and also enables us to take advantage of new advances in trace events that are happening in Chrome.

## Links

* [Perfetto Website](https://www.perfetto.dev/)
* [Perfetto Source Code](https://android.googlesource.com/platform/external/perfetto/)
* [Node.js Diagnostics WG Tracking Issue](https://github.com/nodejs/diagnostics/issues/277)

## Building

Just like the main Node project, this project is buildable with `make`. This has only been tested on macOS.
