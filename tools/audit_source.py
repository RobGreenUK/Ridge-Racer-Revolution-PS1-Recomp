#!/usr/bin/env python3
"""Check the Git index and reachable root history before source publication.

This is a content-exclusion check, not a legal certification. Review provenance
and licences as well. Run after staging; ignored working files are not inspected.
"""
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
FILES = set(""".gitignore .gitattributes .gitmodules README.md LICENSE THIRD_PARTY_NOTICES.md
CMakeLists.txt VERSION game.toml game_options.toml symbols.toml psx_symbols.h
codegen_setup.c codegen_setup.h catalog_identity.json disc_manifest.json disc_probe.json
framework_pins.txt""".split())
FILES.add('Ridge Racer Revolution.command')
DOCS = {
    'docs/BUILD_MACOS.md', 'docs/BUILD_WINDOWS.md', 'docs/PLAY.md',
    'docs/TECHNICAL_OVERVIEW.md', 'docs/ARCHITECTURE.md',
    'docs/DEVELOPMENT.md', 'docs/RR_COMPARISON.md',
}
PREFIXES = ('src/', 'launcher/', 'scripts/', 'tools/', 'tests/', 'cmake/', 'patches/', 'seeds/', 'mods/preloaded/')
EXTENSIONS = {'.c', '.h', '.cpp', '.py', '.sh', '.ps1', '.cmd', '.swift', '.cmake', '.toml', '.txt', '.md', '.patch'}
ASSETS = {'assets/psxrecomp.ico', 'assets/psxrecomp.png', 'assets/psxrecomp.svg'}
DEPS = {'psxrecomp': 'd3e91e07b56c0b672be6136e9fce6af541e9d1e9',
        'recomp-ui': '028fa5c238265090a6596d1256168bb0b69b0e60'}

def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT)

def permitted(name):
    return name in FILES | DOCS | ASSETS or (name.startswith(PREFIXES) and
        (Path(name).suffix in EXTENSIONS or name == 'mods/preloaded/packages/.gitkeep'))

def audit():
    errors = []
    checked = set()
    def entries(data):
        for line in data.split(b'\0'):
            if not line: continue
            metadata, rawname = line.split(b'\t', 1)
            fields = metadata.decode().split()
            mode, oid = fields[0], fields[1] if len(fields[1]) == 40 else fields[2]
            name = rawname.decode()
            if mode == '160000':
                if DEPS.get(name) != oid: errors.append(f'Unapproved dependency: {name}')
                continue
            if not permitted(name): errors.append(f'Excluded/unreviewed path: {name}'); continue
            if mode not in ('100644', '100755'): errors.append(f'Unexpected file mode: {name}'); continue
            if oid in checked: continue
            checked.add(oid)
            content = git('cat-file', 'blob', oid)
            if len(content) > 1_000_000: errors.append(f'Oversized source file: {name}')
            if name in ASSETS: continue
            try: text = content.decode('utf-8')
            except UnicodeDecodeError: errors.append(f'Non-text source: {name}'); continue
            if '\0' in text: errors.append(f'Binary content: {name}')
            if re.search(r'/Users/[\w.-]+/|/home/[\w.-]+/|[A-Z]:\\Users\\[\w.-]+\\', text):
                errors.append(f'Private absolute path: {name}')
            if re.search(r'-----BEGIN (?:RSA |OPENSSH |EC )?PRIVATE KEY-----|gh[pousr]_[A-Za-z0-9]{30,}', text):
                errors.append(f'Possible secret: {name}')
    entries(git('ls-files', '--stage', '-z'))
    commits = git('rev-list', '--all').decode().splitlines()
    for commit in commits:
        entries(git('ls-tree', '-r', '-z', commit))
    if errors:
        print('\n'.join(sorted(set(errors))), file=sys.stderr)
        return 1
    print(f'Source exclusion check passed: {len(checked)} blobs; {len(commits)} root commits. Dependencies retain their own history/licences.')
    return 0

if __name__ == '__main__':
    sys.exit(audit())
