import os
import sys
from pathlib import Path


def main():
    paths = sys.argv[1:]
    for source, destination in zip(paths[::2], paths[1::2]):
        source, destination = Path(source), Path(destination)
        destination.parent.mkdir(parents=True, exist_ok=True)
        if destination.exists() and not destination.is_symlink():
            raise SystemExit('Refusing to replace an existing file: ' + str(destination))
        relative = os.path.relpath(source, destination.parent)
        if destination.is_symlink() and os.readlink(destination) == relative:
            continue
        temporary = destination.with_name(destination.name + '.meson-link')
        temporary.symlink_to(relative)
        temporary.replace(destination)


if __name__ == '__main__':
    main()
