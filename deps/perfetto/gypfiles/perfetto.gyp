{
  'includes': ['perfetto_gen.gypi'],
  'targets': [
    {
      'target_name': 'libperfetto',
      'direct_dependent_settings': {
        'include_dirs': [
          '../include',
          '<(SHARED_INTERMEDIATE_DIR)/gen/protos/',
          '../../protobuf/src'
        ],
        'defines': [
          'GOOGLE_PROTOBUF_NO_RTTI',
        ],
      },
      'sources': [
        
      ],
      'dependencies': [
        'src_tracing_client_api',
        'src_tracing_platform_posix',
        'protos_perfetto_trace_lite',
        'protos_perfetto_trace_chrome_minimal_complete_lite',
        'protos_perfetto_trace_chrome_zero',
        'protobuf.gyp:buildtools_protobuf_lite',
      ],
      'type': 'none'
    }
  ]
}
