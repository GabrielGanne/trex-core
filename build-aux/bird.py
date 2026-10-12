import os
import shutil
import sys
from pathlib import Path


if sys.argv[1] == 'install':
    source, destination = map(Path, sys.argv[2:])
    if destination.is_absolute():
        destination = Path(os.environ.get('DESTDIR') or '/') / destination.relative_to('/')
    else:
        destination = Path(os.environ['MESON_INSTALL_DESTDIR_PREFIX']) / destination
    destination.mkdir(parents=True, exist_ok=True)
    for name in ['trex_bird', 'bird.conf', 'birdcl']:
        if (source / name).is_file():
            shutil.copy2(source / name, destination / name)
else:
    root, stamp = map(Path, sys.argv[1:])
    sys.path.insert(0, str(root / 'linux_dpdk'))
    import compile_bird

    compile_bird.build_bird(str(stamp.resolve().parent))
    compile_bird.create_links(str(stamp.resolve().parent / 'bird'), str(root / 'scripts/bird'))
    stamp.write_text('built\n')
