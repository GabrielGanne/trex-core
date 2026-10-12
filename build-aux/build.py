import argparse
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent


def run(command):
    subprocess.run(command, cwd=ROOT, check=True)


def main():
    component = sys.argv[1]
    parser = argparse.ArgumentParser(description='TRex Meson build launcher')
    parser.add_argument('commands', nargs='*', default=['build'], choices=['configure', 'build', 'clean', 'install', 'list', 'test', 'build_info', 'pkg', 'run'])
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--sanitized', action='store_true')
    parser.add_argument('--no-mlx', nargs='?', const='all', choices=['all', 'mlx5', 'mlx4'])
    parser.add_argument('--no-bnxt', action='store_true')
    parser.add_argument('--no-old', action='store_true')
    parser.add_argument('--no-ver', action='store_true', help='Version files are generated only during Meson setup')
    parser.add_argument('--no-ofed-check', action='store_true', help='Meson probes headers directly; an OFED version check is unnecessary')
    for name in ['mana', 'ntacc', 'bird', 'archive']:
        parser.add_argument('--with-' + name, action='store_true')
    parser.add_argument('--new-memory', action='store_true')
    parser.add_argument('--tap', action='store_true')
    compiler = parser.add_mutually_exclusive_group()
    for version in ['6', '7', '8']:
        compiler.add_argument('--gcc' + version, action='store_true')
    parser.add_argument('--targets', default='')
    parser.add_argument('--build-modes', default='debug,release')
    parser.add_argument('--pkg-dir', '--pkg_dir', type=Path)
    parser.add_argument('--pkg-file', '--pkg_file')
    parser.add_argument('-j', '--jobs', type=int)
    parser.add_argument('-v', '--verbose', action='store_true')
    args, meson_options = parser.parse_known_args(sys.argv[2:])
    if any(not option.startswith('-D') and not option.startswith('--native-file=') and not option.startswith('--cross-file=') for option in meson_options):
        parser.error('Additional setup options must use Meson -Dname=value, --native-file=..., or --cross-file=...')
    build_dir = args.build_dir or ROOT / {'dpdk': 'linux_dpdk/build_dpdk', 'simulator': 'linux/build', 'docs': 'doc/build'}[component]
    build_dir = build_dir.resolve()
    for command in args.commands or ['build']:
        if command == 'configure':
            options = ['-Ddpdk=' + str(component == 'dpdk').lower(), '-Dsimulator=' + str(component == 'simulator').lower(), '-Ddocs=' + str(component == 'docs').lower(), '-Dbuild_modes=' + args.build_modes]
            if args.sanitized:
                options += ['-Db_sanitize=address']
            if args.no_mlx in ['all', 'mlx5']:
                options += ['-Dmlx5=disabled']
            if args.no_bnxt:
                options += ['-Dbnxt=false']
            if args.no_old:
                options += ['-Dold_targets=false']
            for name in ['mana', 'ntacc', 'bird', 'archive']:
                if getattr(args, 'with_' + name):
                    options += ['-D' + name + '=true']
            if args.new_memory:
                options += ['-Dnew_memory=true']
            if args.tap:
                options += ['-Dtap=true']
            for version in ['6', '7', '8']:
                if getattr(args, 'gcc' + version):
                    compiler_paths = []
                    for program in ['gcc', 'g++']:
                        path = shutil.which(program + '-' + version)
                        if path is None:
                            candidates = [Path('/usr/local/gcc-' + {'6': '6.2', '7': '7.4', '8': '8.3'}[version]) / 'bin' / program, Path('/opt/rh/devtoolset-' + version + '/root/usr/bin') / program]
                            path = next((str(p) for p in candidates if p.is_file()), None)
                        if path is None:
                            parser.error('Cannot find ' + program + ' version ' + version + '; use a Meson native file to select a compiler')
                        compiler_paths.append(path)
                    native_file = build_dir.parent / ('meson-gcc' + version + '.ini')
                    native_file.parent.mkdir(parents=True, exist_ok=True)
                    native_file.write_text('[binaries]\nc = ' + repr(compiler_paths[0]) + '\ncpp = ' + repr(compiler_paths[1]) + '\n')
                    options += ['--native-file=' + str(native_file)]
            setup = ['meson', 'setup', str(build_dir), str(ROOT)]
            if (build_dir / 'meson-private/coredata.dat').exists():
                setup += ['--reconfigure']
            run(setup + options + meson_options)
        elif command in ['build', 'clean']:
            compile_args = ['meson', 'compile', '-C', str(build_dir)]
            if command == 'clean':
                compile_args += ['--clean']
            if args.jobs:
                compile_args += ['-j', str(args.jobs)]
            if args.verbose:
                compile_args += ['-v']
            run(compile_args + [target for target in args.targets.split(',') if target])
        elif command == 'test':
            run(['meson', 'test', '-C', str(build_dir), '--print-errorlogs'])
        elif command == 'install':
            run(['meson', 'install', '-C', str(build_dir)])
        elif command in ['build_info', 'list']:
            run(['meson', 'configure', str(build_dir)] if command == 'build_info' else ['meson', 'introspect', '--targets', str(build_dir)])
        elif command == 'pkg' and component == 'dpdk':
            package_args = [sys.executable, str(ROOT / 'build-aux/package.py'), str(ROOT), str(build_dir)]
            if args.pkg_dir:
                package_args += ['--pkg-dir', str(args.pkg_dir.resolve())]
            if args.pkg_file:
                package_args += ['--pkg-file', args.pkg_file]
            run(package_args)
        elif command == 'run' and component == 'docs':
            run([sys.executable, '-m', 'http.server', '8080', '--bind', '127.0.0.1', '--directory', str(build_dir / 'doc')])
        else:
            parser.error(command + ' is unavailable for ' + component)


if __name__ == '__main__':
    try:
        main()
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode)
