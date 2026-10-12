config_dir = get_option('dpdk_config')
if config_dir == ''
  config_dir = 'src/pal/linux_dpdk/dpdk_2507_' + arch
endif
config_dir = source_root / config_dir
if not fs.is_file(config_dir / 'rte_config.h')
  error('Missing rte_config.h in ' + config_dir + '; supply the target configuration with -Ddpdk_config=...')
endif

build_mlx5 = not get_option('mlx5').disabled() and arch != 'aarch64'
if arch == 'aarch64' and get_option('mlx5').enabled()
  error('The bundled TRex build does not support the MLX5 PMD on aarch64')
endif
if get_option('ntacc') and not fs.is_file(source_root / 'src/dpdk/drivers/net/ntacc/rte_eth_ntacc.c')
  error('Napatech PMD sources are absent from this checkout; restore src/dpdk/drivers/net/ntacc before enabling -Dntacc=true')
endif
subdir('sources')
subdir('cpu')

if config_dir.startswith(source_root + '/')
  config_includes = include_directories('../' + config_dir.split(source_root + '/')[1])
else
  config_includes = include_directories(config_dir)
endif
dpdk_includes = [config_includes, include_directories('.')]
arch_include = {'ppc64le': 'ppc', 'aarch64': 'arm', 'x86_64': 'x86'}[arch]
dpdk_includes += include_directories('../src/dpdk/lib/eal/' + arch_include + '/include')
foreach path : dpdk_include_paths
  path = path.replace('@ARCH@', arch)
  if fs.is_dir(source_root / path)
    dpdk_includes += include_directories('../' + path)
  endif
endforeach
app_includes = include_directories('../src', '../src/drivers', '../src/pal/common', '../src/pal/linux_dpdk',
  '../src/stx', '../src/stx/common', '../src/utils', '../src/hdrh', '../external_libs/yaml-cpp/include',
  '../external_libs/zmq/' + arch + '/include', '../external_libs/json', '../external_libs/md5',
  '../external_libs/bpf', '../external_libs/valijson/include')
tcp_includes = include_directories('../src/44bsd/netinet')
bpf_includes = include_directories('../external_libs/bpf', '../external_libs/bpf/bpfjit')

dpdk_flags = ['-DTAP_MAX_QUEUES=16', '-DPF_DRIVER', '-DVF_DRIVER', '-DINTEGRATED_VF', '-include', config_dir / 'rte_config.h']
if arch == 'x86_64' or arch == 'ppc64le'
  dpdk_flags += ['-DX722_SUPPORT', '-DX722_A0_SUPPORT']
endif
dpdk_deps = system_deps
app_flags = []
if get_option('new_memory')
  dpdk_deps += cc.find_library('numa')
  memory_config = configuration_data()
  memory_config.set('RTE_EAL_NUMA_AWARE_HUGEPAGES', 1)
  configure_file(output: 'dpdk_config.h', configuration: memory_config)
  dpdk_flags += ['-include', meson.current_build_dir() / 'dpdk_config.h']
  app_flags += ['-DDPDK_NEW_MEMORY']
endif
if get_option('archive')
  dpdk_deps += dependency('libarchive')
  dpdk_flags += ['-DRTE_HAS_LIBARCHIVE']
endif

zmq_dir = source_root / 'external_libs/zmq' / arch
zmq = cc.find_library('zmq', dirs: zmq_dir)
configure_file(input: '../VERSION', output: 'version.h',
  command: [python, files('../build-aux/version.py'), '@INPUT@', '@OUTPUT@'])
version_source = configure_file(input: 'version.c.in', output: 'version.c', copy: true)
app_sources = main_src + cmn_src + net_src + yaml_src + json_src + md5_src + rpc_server_src + stx_src + stf_src + stateless_src + astf_batch_src + astf_src + version_source
dpdk_sources = dpdk_src
bpf_sources = bpf_src
if arch == 'x86_64'
  dpdk_sources += i40e_dpdk_src + dpdk_src_x86_64 + dpdk_src_x86_64_ext
  bpf_sources += bpfjit_src
  if get_option('tap')
    dpdk_sources += dpdk_src_x86_64_tap
    tap_config = configuration_data()
    foreach probe : tap_probes
      tap_config.set(probe[0], cc.has_header_symbol(probe[1], probe[3]))
    endforeach
    configure_file(output: 'tap_autoconf.h', configuration: tap_config)
    if cc.has_header_symbol('linux/pkt_cls.h', 'TCA_FLOWER_ACT')
      dpdk_flags += ['-DHAVE_TCA_FLOWER']
    endif
  endif
elif arch == 'aarch64'
  dpdk_sources += dpdk_src_aarch64
else
  dpdk_sources += i40e_dpdk_src + dpdk_src_ppc64le
  bpf_sources += bpfjit_src
endif

verbs_includes = []
verbs_deps = []
if build_mlx5 or get_option('mana')
  ibverbs = dependency('libibverbs', required: false)
  mlx5 = cc.find_library('mlx5', required: false)
  system_verbs = ibverbs.found() and cc.has_header('infiniband/verbs.h', dependencies: ibverbs) and (not build_mlx5 or mlx5.found())
  if not system_verbs
    verbs_dir = source_root / 'external_libs/ibverbs' / arch
    verbs_includes += include_directories('../external_libs/ibverbs/' + arch + '/include')
    ibverbs = cc.find_library('ibverbs', dirs: verbs_dir)
    mlx5 = cc.find_library('mlx5', dirs: verbs_dir)
  endif
  verbs_deps = [ibverbs]
  if build_mlx5
    verbs_deps += mlx5
    if system_verbs
      mlx5_config = configuration_data()
      foreach probe : mlx5_probes
        prefix = '#include <' + probe[1] + '>'
        if probe[2] == 'field'
          member = probe[3].split('.')
          found = cc.has_member(member[0], member[1], prefix: prefix, dependencies: verbs_deps)
        elif probe[2] == 'type' and probe[3].startswith('struct ')
          found = cc.has_type(probe[3], prefix: prefix, dependencies: verbs_deps)
        else
          found = cc.has_header_symbol(probe[1], probe[3], dependencies: verbs_deps)
        endif
        if found
          mlx5_config.set(probe[0], 1)
        endif
      endforeach
      configure_file(output: 'mlx5_autoconf.h', configuration: mlx5_config)
    else
      configure_file(input: '../build-aux/mlx5_autoconf.h', output: 'mlx5_autoconf.h', copy: true)
    endif
    mnl = dependency('libmnl', required: false)
  endif
endif
mana_deps = []
if get_option('mana')
  mana_deps = [ibverbs, cc.find_library('mana')]
endif
ntacc_deps = []
if get_option('ntacc')
  ntacc_deps = [cc.find_library('ntapi', dirs: '/opt/napatech3/lib')]
  verbs_includes += include_directories('/opt/napatech3/include')
endif

old_variants = get_option('old_targets') ? [false, true] : [false]
foreach old : old_variants
  foreach mode : get_option('build_modes')
    suffix = mode == 'debug' ? '-64-debug' : '-64'
    if old
      suffix += '-o'
    endif
    cpu_flags = old ? cpu_flags_old : cpu_flags_new
    cflags = dpdk_common_flags + cpu_flags
    cppflags = cflags + warning_flags + app_flags
    overrides = ['optimization=' + (mode == 'debug' ? '0' : '3'), 'debug=true', 'b_ndebug=false']
    if mode == 'debug'
      cflags += debug_flags
      cppflags += debug_flags
    else
      cflags += ['-DNDEBUG']
    endif
    if cxx.get_id() == 'gcc' and arch == 'x86_64'
      cppflags += ['-mrtm']
    endif
    dpdk = static_library('dpdk' + suffix, dpdk_sources, include_directories: dpdk_includes,
      c_args: cflags + dpdk_flags, override_options: overrides, pic: false)
    tcp = static_library('tcp' + suffix, tcp_src, include_directories: tcp_includes,
      c_args: cflags, override_options: overrides, pic: false)
    bpf = shared_library('bpf' + suffix, bpf_sources, include_directories: bpf_includes,
      c_args: cflags + ['-DSLJIT_CONFIG_AUTO=1', '-DINET6'], override_options: overrides,
      install: true, install_dir: runtime_dir / 'so' / arch)
    trex = executable('_t-rex' + suffix, app_sources,
      include_directories: [dpdk_includes, app_includes, tcp_includes],
      c_args: cflags + dpdk_flags, cpp_args: cppflags + dpdk_flags,
      link_whole: [dpdk, tcp], link_with: bpf, dependencies: dpdk_deps + [zmq],
      export_dynamic: true, override_options: overrides,
      build_rpath: '$ORIGIN:' + zmq_dir,
      install_rpath: '$ORIGIN/so:$ORIGIN/so/' + arch, install: true, install_dir: runtime_dir)
    stage_links += [trex.full_path(), source_root / 'scripts' / ('_t-rex' + suffix),
      bpf.full_path(), source_root / 'scripts/so' / arch / ('libbpf' + suffix + '.so')]
    pmds = []
    pmd_overrides = overrides + ['b_lundef=false']
    if build_mlx5
      mnl_deps = []
      mnl_libs = []
      mnl_includes = []
      if mnl.found()
        mnl_deps += mnl
      else
        mnl_includes += include_directories('../external_libs/libmnl/include')
        mnl_lib = shared_library('mnl' + suffix, libmnl_src, include_directories: mnl_includes,
          c_args: cflags, override_options: overrides, install: true, install_dir: runtime_dir / 'so' / arch)
        mnl_libs += mnl_lib
        pmds += mnl_lib
      endif
      mlx5_flags = ['-D_BSD_SOURCE', '-D_DEFAULT_SOURCE', '-D_XOPEN_SOURCE=600', '-D_FILE_OFFSET_BITS=64']
      if mode == 'debug'
        mlx5_flags += ['-UNDEBUG', '-DRTE_LIBRTE_MLX5_DEBUG']
      endif
      pmds += shared_library('mlx5' + suffix, mlx5_dpdk_src,
        include_directories: [dpdk_includes, verbs_includes, mnl_includes],
        c_args: cflags + dpdk_flags + mlx5_flags, dependencies: verbs_deps + mnl_deps,
        link_with: mnl_libs, override_options: pmd_overrides + ['c_std=c11'],
        build_rpath: '$ORIGIN', install_rpath: '$ORIGIN', install: true, install_dir: runtime_dir / 'so' / arch)
    endif
    if get_option('bnxt')
      pmds += shared_library('bnxt' + suffix, bnxt_dpdk_src, include_directories: dpdk_includes,
        c_args: cflags + dpdk_flags, override_options: pmd_overrides,
        install: true, install_dir: runtime_dir / 'so' / arch)
    endif
    if get_option('mana')
      pmds += shared_library('mana' + suffix, mana_dpdk_src, include_directories: [dpdk_includes, verbs_includes],
        c_args: cflags + dpdk_flags, dependencies: mana_deps, override_options: pmd_overrides,
        install: true, install_dir: runtime_dir / 'so' / arch)
    endif
    if get_option('ntacc')
      pmds += shared_library('ntacc' + suffix, ntacc_dpdk_src, include_directories: [dpdk_includes, verbs_includes],
        c_args: cflags + dpdk_flags + ['-DNAPATECH3_LIB_PATH="/opt/napatech3/lib"'], dependencies: ntacc_deps,
        override_options: pmd_overrides, install: true, install_dir: runtime_dir / 'so' / arch)
    endif
    foreach pmd : pmds
      stage_links += [pmd.full_path(), source_root / 'scripts/so' / arch / ('lib' + pmd.name() + '.so')]
    endforeach
  endforeach
endforeach
if get_option('bird')
  bird = custom_target('bird', output: 'bird.stamp',
    input: ['compile_bird.py', 'BIRD_VERSION'], command: [python, files('../build-aux/bird.py'), source_root, '@OUTPUT@'],
    build_by_default: true)
  meson.add_install_script(python, files('../build-aux/bird.py'), 'install',
    meson.current_build_dir() / 'bird', runtime_dir / 'bird')
endif
run_target('pkg', command: [python, files('../build-aux/package.py'), source_root, build_root])
