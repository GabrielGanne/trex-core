project('trex', 'c', 'cpp',
  version: run_command('python3', '-c', 'from pathlib import Path; print(Path("VERSION").read_text().strip())', check: true).stdout().strip(),
  meson_version: '>=0.61.0',
  default_options: ['buildtype=plain', 'cpp_std=gnu++11', 'c_std=gnu11', 'warning_level=0'])

fs = import('fs')
python = find_program('python3')
cc = meson.get_compiler('c')
cxx = meson.get_compiler('cpp')
arch = host_machine.cpu_family()
if arch == 'ppc64'
  arch = 'ppc64le'
endif
if host_machine.system() != 'linux' or host_machine.endian() != 'little' or not ['x86_64', 'aarch64', 'ppc64le'].contains(arch)
  error('TRex requires Linux on x86_64, aarch64, or little endian ppc64')
endif

source_root = meson.project_source_root()
build_root = meson.project_build_root()
runtime_dir = get_option('datadir') / 'trex'
stage_script = files('build-aux/stage.py')
stage_links = []
version = fs.read('VERSION').strip()
system_deps = []
if get_option('dpdk') or get_option('simulator')
  system_deps = [dependency('threads'), dependency('zlib'), cc.find_library('dl'), cc.find_library('rt')]
endif

debug_flags = ['-D_DEBUG']
common_flags = ['-DWIN_UCODE_SIM', '-D_BYTE_ORDER', '-D_LITTLE_ENDIAN', '-DLINUX', '-D__STDC_LIMIT_MACROS', '-D__STDC_FORMAT_MACROS']
warning_flags = ['-Wno-deprecated-declarations', '-Wno-sign-compare', '-Wno-strict-aliasing', '-Wno-address-of-packed-member', '-Wno-error=uninitialized']
if cxx.get_id() == 'clang'
  warning_flags += ['-Wno-null-conversion', '-Wno-inconsistent-missing-override', '-Wno-deprecated', '-Wno-unused-private-field', '-Wno-unused-value', '-Wno-expansion-to-defined', '-Wno-macro-redefined']
else
  warning_flags = ['-Wall'] + warning_flags + ['-Wno-literal-suffix', '-Wno-aligned-new']
endif
warning_flags = cxx.get_supported_arguments(warning_flags)
if get_option('dpdk')
  subdir('build-aux/mlx5-probes')
  mlx5_probes = probes
  subdir('build-aux/tap-probes')
  tap_probes = probes
  subdir('linux_dpdk')
endif
if get_option('simulator')
  subdir('linux')
endif
if get_option('docs')
  subdir('doc')
endif
if get_option('script_links') and stage_links.length() > 0
  run_command(python, stage_script, stage_links, check: true)
endif
if get_option('dpdk') or get_option('simulator')
  runtime_excludes = ['bp-sim-64', 'bp-sim-64-debug', '_t-rex-64', '_t-rex-64-debug', '_t-rex-64-o', '_t-rex-64-debug-o',
    'so/x86_64/libstdc++.so.6', 'so/aarch64/libstdc++.so.6', 'so/ppc64le/libstdc++.so.6']
  foreach suffix : ['-64', '-64-debug', '-64-o', '-64-debug-o']
    foreach library : ['bpf', 'mnl', 'mlx4', 'mlx5', 'bnxt', 'mana', 'ntacc']
      runtime_excludes += 'so/' + arch + '/lib' + library + suffix + '.so'
    endforeach
  endforeach
  install_subdir('scripts', install_dir: runtime_dir, strip_directory: true,
    exclude_directories: ['generated', 'doc', 'bird', '__pycache__', 'automation/regression/reports'],
    exclude_files: runtime_excludes)
endif

summary({'Version': version, 'Architecture': arch, 'TRex': get_option('dpdk'),
  'Simulator': get_option('simulator'), 'Documentation': get_option('docs'),
  'Variants': get_option('build_modes'), 'Links in scripts': get_option('script_links')}, section: 'TRex')
