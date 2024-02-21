src = [
  'eval.cc',
  'flutsch.cc'
]

deps = [
      nix_main_dep,
      nix_store_dep,
      nix_expr_dep,
      nix_cmd_dep,
      boost_dep,
      nlohmann_json_dep,
      threads_dep
    ]

lib_flutsch_headers = include_directories('include')

lib_flutsch = shared_library('lib-flutsch', src,
    dependencies : deps,
    install : true,
    include_directories : lib_flutsch_headers,
    )

# lib_flutsch_dep = declare_dependency(
#     include_directories : include_directories('.'),
#     dependencies:  deps, link_with : lib_flutsch
#   )



