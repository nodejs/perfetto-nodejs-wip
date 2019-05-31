{
  'includes': ['perfetto_gen.gypi'],
  'targets': [
    {
      'target_name': 'libperfetto',
      'direct_dependent_settings': {
        'include_dirs': [
          '../include',
          '<(SHARED_INTERMEDIATE_DIR)/gen/protos/'
        ]
      },
      'sources': [
        
      ],
      'dependencies': [
        'src_public_public',
        'src_public_platform_posix'
      ],
      'type': 'none'
    }
  ]
}
