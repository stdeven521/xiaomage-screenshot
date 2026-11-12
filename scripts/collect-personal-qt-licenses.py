#!/usr/bin/env python3
"""Collect the pinned official Qt source-license metadata for a shared macOS kit."""

import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import sys
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request


ROOT = Path(__file__).resolve().parents[1]
MODULES = ('qt5', 'qtbase', 'qtsvg', 'qttools', 'qttranslations')
MARKER = 'complete.json'


class SafeRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, message, headers, new_url):
        redirected = super().redirect_request(request, fp, code, message, headers, new_url)
        if redirected and urllib.parse.urlparse(new_url).hostname != 'api.github.com':
            redirected.remove_header('Authorization')
        return redirected


def download(url):
    parsed = urllib.parse.urlparse(url)
    if parsed.scheme != 'https' or parsed.hostname not in ('api.github.com', 'raw.githubusercontent.com'):
        raise ValueError('Qt metadata must come from the official GitHub API or raw content host')
    headers = {'User-Agent': 'xiaomage-screenshot-qt-license-collector'}
    if parsed.hostname == 'api.github.com':
        headers['Accept'] = 'application/vnd.github+json'
        token = os.environ.get('GITHUB_TOKEN')
        if token:
            headers['Authorization'] = 'Bearer ' + token
    request = urllib.request.Request(url, headers=headers)
    for attempt in range(3):
        try:
            with urllib.request.build_opener(SafeRedirect()).open(request, timeout=60) as response:
                return response.read()
        except (urllib.error.URLError, TimeoutError):
            if attempt == 2:
                raise
            time.sleep(attempt + 1)


def github_tree(repository, reference):
    url = f'https://api.github.com/repos/qt/{repository}/git/trees/{reference}'
    result = json.loads(download(url))
    if result.get('truncated') or not isinstance(result.get('tree'), list):
        raise ValueError(f'Incomplete official Qt tree: {repository}/{reference}')
    return result['tree']


def collect_entries(version):
    entries = []
    for module in MODULES:
        tree = {entry['path']: entry for entry in github_tree(module, 'v' + version)}
        metadata = 'licenseRule.json' if module == 'qttranslations' else 'REUSE.toml'
        if tree.get(metadata, {}).get('type') != 'blob' or tree.get('LICENSES', {}).get('type') != 'tree':
            raise ValueError(f'Missing official Qt license metadata: {module} v{version}')
        files = [(metadata, tree[metadata]['sha'])]
        licenses = github_tree(module, tree['LICENSES']['sha'])
        if not licenses:
            raise ValueError(f'Empty official Qt license directory: {module}')
        for entry in licenses:
            name = PurePosixPath(entry['path'])
            if entry['type'] != 'blob' or len(name.parts) != 1 or name.name in ('.', '..'):
                raise ValueError(f'Unexpected Qt license entry: {module}/{entry["path"]}')
            files.append(('LICENSES/' + name.name, entry['sha']))
        for relative, blob_sha in files:
            destination = ('root' if module == 'qt5' else module) + '/' + relative
            url = f'https://raw.githubusercontent.com/qt/{module}/v{version}/' + urllib.parse.quote(relative)
            entries.append((destination, url, blob_sha))
    return entries


def valid_cache(destination, version):
    try:
        receipt = json.loads((destination / MARKER).read_text(encoding='utf-8'))
        if receipt.get('schemaVersion') != 1 or receipt.get('qtVersion') != version:
            return False
        files = receipt['files']
        for module in MODULES:
            prefix = ('root' if module == 'qt5' else module) + '/'
            metadata = 'licenseRule.json' if module == 'qttranslations' else 'REUSE.toml'
            if prefix + metadata not in files or not any(name.startswith(prefix + 'LICENSES/') for name in files):
                return False
        for relative, expected in files.items():
            path = destination / relative
            if path.is_symlink() or not path.is_file() or not path.resolve().is_relative_to(destination.resolve()):
                return False
            if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
                return False
        return True
    except (OSError, ValueError, KeyError, TypeError):
        return False


def collect(prefix):
    version = json.loads((ROOT / 'scripts/qt-toolchain.json').read_text(encoding='utf-8'))['qtVersion']
    parent = (prefix / 'share/snow-apps').resolve()
    destination = parent / 'qt-licenses'
    if destination.is_symlink():
        raise ValueError('Refusing to replace a symlinked Qt license directory')
    if valid_cache(destination, version):
        print(f'Verified Qt {version} license cache: {destination}')
        return
    entries = collect_entries(version)
    parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.qt-licenses-', dir=parent) as temporary:
        stage = Path(temporary)

        def fetch(entry):
            relative, url, expected_blob = entry
            content = download(url)
            blob = b'blob ' + str(len(content)).encode('ascii') + b'\0' + content
            if hashlib.sha1(blob).hexdigest() != expected_blob:
                raise ValueError(f'Official Qt metadata hash mismatch: {relative}')
            target = stage / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(content)
            return relative, hashlib.sha256(content).hexdigest()

        with ThreadPoolExecutor(max_workers=8) as executor:
            files = dict(executor.map(fetch, entries))
        sbom = prefix / 'sbom'
        if sbom.is_dir():
            for source in sorted(sbom.rglob('*')):
                if source.is_file() and not source.is_symlink():
                    relative = 'sbom/' + source.relative_to(sbom).as_posix()
                    target = stage / relative
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copyfile(source, target)
                    files[relative] = hashlib.sha256(target.read_bytes()).hexdigest()
        receipt = {
            'schemaVersion': 1, 'qtVersion': version,
            'source': 'Official qt GitHub repositories at v' + version,
            'files': dict(sorted(files.items())),
        }
        (stage / MARKER).write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        if not valid_cache(stage, version):
            raise ValueError('The completed Qt license bundle failed verification')
        # Only the fixed, resolved child of the requested kit is replaceable.
        if destination.exists():
            if destination.resolve().parent != parent or not destination.is_dir():
                raise ValueError('Refusing to replace an unexpected Qt license path')
            shutil.rmtree(destination)
        stage.replace(destination)
    print(f'Collected and verified {len(files)} Qt {version} license files: {destination}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qt-prefix', type=Path, required=True)
    args = parser.parse_args()
    try:
        collect(args.qt_prefix.resolve())
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f'Qt license collection failed: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
