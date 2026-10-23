{
  "targets": [
    {
      "target_name": "equihashverify",
      "sources": [
        "crypto/equihash.cpp",
        "crypto/equihash_200_9.cpp",
        "crypto/equihash_avx2.cpp",
        "util.cpp",
        "equihashverify.cc"
      ],
      "include_dirs": [
        "<!(node -e \"require('nan')\")",
        "."
      ],
      "cflags_cc!": [
        "-fno-exceptions"
      ],
      "cflags_cc": [
        "-std=c++20",
        "-fexceptions",
        "-UNDEBUG"
      ],
      "libraries": [
        "-lsodium"
      ],
      "conditions": [
        [
          "OS==\"mac\"",
          {
            "include_dirs": [
              "/opt/homebrew/include",
              "/usr/local/include"
            ],
            "libraries": [
              "-L/opt/homebrew/lib",
              "-L/usr/local/lib"
            ],
            "xcode_settings": {
              "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
              "CLANG_CXX_LANGUAGE_STANDARD": "c++20",
              "GCC_PREPROCESSOR_DEFINITIONS": [
                "$(inherited)"
              ],
              "OTHER_CPLUSPLUSFLAGS": [
                "-UNDEBUG"
              ]
            }
          }
        ]
      ]
    }
  ]
}
