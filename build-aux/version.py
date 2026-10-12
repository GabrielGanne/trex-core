import getpass
import json
import subprocess
import sys
import uuid
from pathlib import Path


source, output = map(Path, sys.argv[1:])
try:
    revision = subprocess.check_output(['git', '-C', str(source.parent), 'rev-parse', 'HEAD'], stderr=subprocess.DEVNULL, text=True).strip()
except (OSError, subprocess.CalledProcessError):
    revision = 'N/A'
values = {'USER': getpass.getuser(), 'UIID': str(uuid.uuid4()), 'GIT_SHA': revision, 'BUILD_NUM': source.read_text().strip()}
header = '#ifndef __TREX_VER_FILE__\n#define __TREX_VER_FILE__\n'
header += ''.join('#define VERSION_' + key + ' ' + json.dumps(value) + '\n' for key, value in values.items())
header += '#ifdef __cplusplus\nextern "C" {\n#endif\n'
header += 'const char *get_build_date(void);\nconst char *get_build_time(void);\n'
header += '#ifdef __cplusplus\n}\n#endif\n#endif\n'
output.write_text(header)
