# This file is automatically generated -- do not edit!
{
  "variables": {
    "root_relative_to_gypfile": ".."
  },
  "targets": [
    {
      "target_name": "gen_empty_cc",
      "type": "none",
      "toolsets": [
        "target",
        "host"
      ],
      "target_conditions": [
        [
          "_toolset==\"target\"",
          {
            "actions": [
              {
                "action_name": "gen_empty_cc_action",
                "inputs": [],
                "outputs": [
                  "<(SHARED_INTERMEDIATE_DIR)/gen/empty.cc"
                ],
                "action": [
                  "touch",
                  "-a",
                  "<(SHARED_INTERMEDIATE_DIR)/gen/empty.cc"
                ]
              }
            ]
          }
        ],
        [
          "_toolset==\"host\"",
          {
            "actions": [
              {
                "action_name": "gen_empty_cc_action",
                "inputs": [],
                "outputs": [
                  "<(SHARED_INTERMEDIATE_DIR)/gcc_like_host/gen/empty.cc"
                ],
                "action": [
                  "touch",
                  "-a",
                  "<(SHARED_INTERMEDIATE_DIR)/gcc_like_host/gen/empty.cc"
                ]
              }
            ]
          }
        ]
      ],
      "dependencies": []
    }
  ]
}