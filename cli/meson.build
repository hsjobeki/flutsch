src = [
  'main.cc'
]

executable('flutsch', src,
           dependencies : [
            nix_main_dep,
            nix_store_dep,
            nix_expr_dep,
            nix_cmd_dep,
            boost_dep,
            nlohmann_json_dep,
            threads_dep
           ],
           link_with: [ lib_flutsch ],
           include_directories: [ lib_flutsch_headers ],
           install: true,
           cpp_args: ['-std=c++2a'])
