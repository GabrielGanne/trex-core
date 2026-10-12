subdir('sources')
sim_sources = bp_sim_main + bp_sim_gtest + main_src + cmn_src + stubs + net_src + yaml_src + json_src + stx_src + stf_src + stateless_src + astf_batch_src + astf_src + md5_src + rpc_server_src
sim_includes = include_directories('../src/pal/linux', '../src/pal/common', '../src', '../src/utils',
  '../src/rpc-server', '../src/hdrh', '../src/stx', '../src/stx/common',
  '../external_libs/json', '../external_libs/md5', '../external_libs/zmq/' + arch + '/include',
  '../external_libs/yaml-cpp/include', '../external_libs/bpf', '../external_libs/valijson/include')
tcp_includes = include_directories('../src/44bsd/netinet')
foreach mode : get_option('build_modes')
  suffix = mode == 'debug' ? '-64-debug' : '-64'
  flags = common_flags + ['-DTREX_SIM']
  overrides = ['optimization=' + (mode == 'debug' ? '0' : '3'), 'debug=true', 'b_ndebug=false']
  if mode == 'debug'
    flags += debug_flags + ['-DDEBUG', '-DSTILE_CPP_ASSERT', '-DSTILE_SHIM_ASSERT']
  endif
  tcp = static_library('tcp' + suffix, tcp_src, include_directories: tcp_includes,
    c_args: flags, override_options: overrides, pic: false)
  sim = executable('bp-sim' + suffix, sim_sources,
    include_directories: [sim_includes, tcp_includes], c_args: flags,
    cpp_args: flags + warning_flags, link_whole: tcp, dependencies: system_deps,
    override_options: overrides,
    install: true, install_dir: runtime_dir)
  stage_links += [sim.full_path(), source_root / 'scripts' / ('bp-sim' + suffix)]
  test('simulator' + suffix, sim, args: ['--ut'], workdir: source_root / 'scripts',
    suite: 'unit', timeout: 300, is_parallel: false)
endforeach
if get_option('script_links') and get_option('build_modes').contains('debug') and get_option('build_modes').contains('release')
  regression = find_program('../scripts/run_regression')
  test('functional', regression, args: ['--func'], workdir: source_root / 'scripts',
    suite: 'functional', env: {'DEBUGINFOD_URLS': ''}, timeout: 1800, is_parallel: false)
endif
