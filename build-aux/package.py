import argparse
import json
import shutil
import subprocess
import tarfile
import tempfile
from pathlib import Path


FILES = ['trex-cfg', 'bp-sim-64', 'bp-sim-64-debug', 't-rex-64-debug-gdb', 'stl-sim', 'astf-sim', 'astf-sim-utl', 'find_python.sh', 'run_regression', 'run_functional_tests', 'dpdk_nic_bind.py', 'dpdk_setup_ports.py', 'doc_process.py', 'trex_daemon_server', 'trex-emu', 'general_daemon_server', 'master_daemon.py', 'astf_schema.json', 'trex-console', 'daemon_server', 'ndr']
DIRECTORIES = ['cap2', 'avl', 'cfg', 'ko', 'automation', 'external_libs', 'stl', 'exp', 'astf', 'x710_ddp', 'trex_emu', 'emu', 'so']
CLIENT_LIBRARIES = ['simple_enum', 'jsonrpclib-pelix-0.4.1', 'pyyaml-3.11', 'pyzmq-ctypes', 'scapy-2.4.3', 'texttable-0.8.4', 'simpy-3.0.10', 'trex-openssl', 'dpkt-1.9.1', 'repoze']


def copy(source, destination):
    if source.is_dir():
        shutil.copytree(source, destination, dirs_exist_ok=True, ignore=shutil.ignore_patterns('*.pyc', '__pycache__', 'libstdc++.so.6', 'lib*-64*.so'))
    else:
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)


def main():
    parser = argparse.ArgumentParser(description='Create a TRex runtime and Python client release archive')
    parser.add_argument('source', type=Path)
    parser.add_argument('build', type=Path)
    parser.add_argument('--pkg-dir', type=Path)
    parser.add_argument('--pkg-file')
    args = parser.parse_args()
    version = (args.source / 'VERSION').read_text().strip()
    destination = (args.pkg_dir or args.source) / (args.pkg_file or version + '.tar.gz')
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists():
        parser.error('Package already exists: ' + str(destination))
    targets = json.loads(subprocess.check_output(['meson', 'introspect', '--targets', str(args.build)], text=True))
    build_options = {option['name']: option['value'] for option in json.loads(subprocess.check_output(['meson', 'introspect', '--buildoptions', str(args.build)], text=True))}
    machines = json.loads(subprocess.check_output(['meson', 'introspect', '--machines', str(args.build)], text=True))
    arch = machines['host']['cpu_family']
    if arch == 'ppc64':
        arch = 'ppc64le'
    binaries = [Path(name) for target in targets if target['type'] == 'executable' for name in target['filename']]
    libraries = [Path(name) for target in targets if target['type'] == 'shared library' for name in target['filename']]
    if not any(binary.name.startswith('_t-rex') for binary in binaries):
        parser.error('The selected build has no TRex executable targets')
    missing = [str(binary) for binary in binaries + libraries if not binary.is_file()]
    if missing:
        parser.error('Build these targets before packaging: ' + ', '.join(missing))
    scripts = args.source / 'scripts'
    with tempfile.TemporaryDirectory(prefix='trex-package-', dir=destination.parent) as temporary:
        staging = Path(temporary) / 'install'
        subprocess.run(['meson', 'install', '-C', str(args.build), '--no-rebuild', '--destdir', str(staging.resolve())], check=True, stdout=subprocess.DEVNULL)
        installed_path = Path(build_options['datadir']) / 'trex'
        if not installed_path.is_absolute():
            installed_path = Path(build_options['prefix']) / installed_path
        installed = staging / installed_path.relative_to('/')
        release = Path(temporary) / version
        release.mkdir()
        for filename in FILES:
            if any(binary.name == filename for binary in binaries):
                continue
            source = scripts / filename
            if not source.exists():
                parser.error('Missing runtime file: ' + str(source) + '; build both simulator variants before packaging')
            copy(source, release / filename)
        for directory in DIRECTORIES + (['bird'] if build_options['bird'] else []):
            source = scripts / directory
            if source.exists():
                copy(source, release / directory)
        for binary in binaries:
            copy(installed / binary.name, release / binary.name)
            if binary.name.startswith('_t-rex'):
                copy(scripts / binary.name[1:], release / binary.name[1:])
                subprocess.run(['strip', str(release / binary.name)], check=True)
        for library in libraries:
            copy(installed / 'so' / arch / library.name, release / 'so' / arch / library.name)
        client = Path(temporary) / 'trex_client'
        for library in CLIENT_LIBRARIES:
            copy(scripts / 'external_libs' / library, client / 'external_libs' / library)
        for directory in ['stf', 'interactive']:
            copy(scripts / 'automation/trex_control_plane' / directory, client / directory)
        copy(scripts / 'stl', client / 'interactive/profiles/stl')
        with tarfile.open(release / ('trex_client_' + version + '.tar.gz'), 'w:gz') as archive:
            archive.add(client, arcname='trex_client')
        (release / 'generated').mkdir()
        with tarfile.open(destination, 'w:gz') as archive:
            archive.add(release, arcname=version)
    if destination.stat().st_size > 290_000_000:
        parser.error('Package exceeds the existing 290 MB limit: ' + str(destination))
    print(destination)


if __name__ == '__main__':
    main()
