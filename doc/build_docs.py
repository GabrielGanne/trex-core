import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

from HTMLInjector import HTMLInjector
from hlt_args import parse_hlt_args
from toc import create_toc_json


def main():
    operation, *args = sys.argv[1:]
    if operation in ['assets', 'sources']:
        root = Path(args[0])
        if operation == 'sources':
            paths = list(root.glob('*.asciidoc')) + list(root.glob('*-docinfo.html'))
        else:
            paths = [root / name for name in ['symbols.lang', 'nginx_if_cfg.txt', 'my_chart.js']]
            for directory in ['images', 'yaml', 'video', 'css']:
                paths += [path for path in (root / directory).rglob('*') if path.is_file()]
        print('\n'.join(str(path.relative_to(root)) for path in sorted(paths)))
    elif operation == 'copy-assets':
        source, destination, stamp, *assets = args
        for asset in assets:
            asset = Path(asset)
            output = Path(destination) / asset.resolve().relative_to(Path(source).resolve())
            output.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(asset, output)
        Path(stamp).write_text('copied\n')
    elif operation == 'install-assets':
        source, destination = args
        destination = Path(destination)
        if destination.is_absolute():
            destination = Path(os.environ.get('DESTDIR') or '/') / destination.relative_to('/')
        else:
            destination = Path(os.environ['MESON_INSTALL_DESTDIR_PREFIX']) / destination
        for name in ['images', 'yaml', 'video', 'css', 'cp_docs', 'cp_stl_docs', 'cp_astf_docs']:
            path = Path(source) / name
            if path.is_dir():
                shutil.copytree(path, destination / name, dirs_exist_ok=True)
        for name in ['symbols.lang', 'nginx_if_cfg.txt', 'my_chart.js']:
            shutil.copy2(Path(source) / name, destination / name)
    elif operation == 'hlt':
        source, output = args
        parse_hlt_args(source, output)
    elif operation == 'html':
        program, source, stylesheet, output, mode, hlt = args
        output = Path(output)
        command = [program, '-a', 'stylesheet=' + str(Path(stylesheet).resolve()), '-a', 'icons=true', '-a', 'max-width=55em', '-a', 'hlt_args=' + str(Path(hlt).resolve())]
        if mode == 'deck':
            command += ['-f', str(Path(__file__).parent / 'backends/deckjs/deckjs.conf')]
        elif mode.startswith('toc') or mode == 'book':
            command += ['-d', 'book']
        if mode in ['toc', 'toc-disqus', 'book']:
            command += ['-a', 'docinfo']
        if mode in ['article-toc', 'book']:
            command += ['-a', 'toc2']
        if mode.startswith('toc'):
            with tempfile.TemporaryDirectory(dir=output.resolve().parent) as temporary:
                intermediate = Path(temporary) / 'document.html'
                subprocess.run(command + ['-o', str(intermediate), source], check=True)
                toc = output.with_suffix('.json')
                create_toc_json(intermediate, toc)
                with HTMLInjector(str(intermediate), str(output)) as injector:
                    injector.inject_toc(toc.name)
                    if mode == 'toc-disqus':
                        injector.inject_disqus(output.name)
        else:
            subprocess.run(command + ['-o', str(output), source], check=True)
    elif operation == 'pdf':
        program, source, output, hlt = args
        subprocess.run([program, '--no-xmllint', '-f', 'pdf', '-d', 'book', '-a', 'hlt_args=' + str(Path(hlt).resolve()), '-D', str(Path(output).resolve().parent), source], check=True)
    elif operation == 'sphinx':
        program, source, output, stamp = args
        subprocess.run([program, '-W', '-b', 'html', '.', str(Path(output).resolve())], cwd=source, check=True)
        Path(stamp).write_text('built\n')


if __name__ == '__main__':
    try:
        main()
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode)
