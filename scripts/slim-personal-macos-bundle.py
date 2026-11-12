#!/usr/bin/env python3
"""Thin and strip only the staged Apple Silicon personal package, then sign nested code."""

import argparse
import json
import os
from pathlib import Path
import platform
import plistlib
import stat
import subprocess
import tempfile


MACHO_MAGIC = {bytes.fromhex(value) for value in (
    'cffaedfe', 'feedfacf', 'cefaedfe', 'feedface',
    'cafebabe', 'cafebabf', 'bebafeca', 'bfbafeca',
)}
CODE_BUNDLES = {'.app', '.framework', '.xpc', '.bundle'}


def run(*command):
    return subprocess.check_output(command, text=True, stderr=subprocess.STDOUT,
                                   timeout=120).strip()


def real_files(directory, *, exclude_dsym=False):
    for current, directories, files in os.walk(directory, followlinks=False):
        parent = Path(current)
        directories[:] = [name for name in directories
                          if not (parent / name).is_symlink()
                          and not (exclude_dsym and name.endswith('.dSYM'))]
        for name in files:
            path = parent / name
            if not path.is_symlink() and path.is_file():
                yield path


def bundle_bytes(app):
    return sum(path.stat().st_size for path in real_files(app))


def slim_bundle(app, staging_root, identity):
    app = app.absolute()
    staging_root = staging_root.resolve()
    if (app.is_symlink() or not app.is_dir() or app.suffix != '.app'
            or staging_root.name != '_CPack_Packages'
            or not app.resolve().is_relative_to(staging_root)):
        raise ValueError('Only an application inside the CPack staging directory may be changed')
    with (app / 'Contents/Info.plist').open('rb') as source:
        executable = plistlib.load(source).get('CFBundleExecutable')
    if executable != 'snow_shot':
        raise ValueError('Expected the full personal application executable')
    main = app / 'Contents/MacOS' / executable
    binaries = []
    # dSYM DWARF files also have Mach-O headers, but are debug companions rather
    # than runtime code. Preserve them; bundle_bytes still counts their storage.
    for path in sorted(real_files(app, exclude_dsym=True)):
        if not path.resolve().is_relative_to(app.resolve()):
            raise ValueError(f'Bundle file escapes staging: {path}')
        with path.open('rb') as source:
            if source.read(4) in MACHO_MAGIC:
                binaries.append(path)
    if main not in binaries:
        raise ValueError('The staged main executable is missing')

    # Reject incompatible files before changing any staged binary.
    architectures = {path: run('/usr/bin/lipo', '-archs', str(path)).split()
                     for path in binaries}
    for path, slices in architectures.items():
        if 'arm64' not in slices:
            raise ValueError(f'Apple Silicon package dependency has no arm64 slice: {path}')

    before = bundle_bytes(app)
    thinned = []
    for path, slices in architectures.items():
        if slices == ['arm64']:
            continue
        original_mode = stat.S_IMODE(path.stat().st_mode)
        with tempfile.NamedTemporaryFile(prefix=path.name + '.thin-', dir=path.parent,
                                         delete=False) as output:
            temporary = Path(output.name)
        try:
            run('/usr/bin/lipo', str(path), '-thin', 'arm64', '-output', str(temporary))
            if run('/usr/bin/lipo', '-archs', str(temporary)).split() != ['arm64']:
                raise ValueError(f'Thinning did not produce an arm64 binary: {path}')
            temporary.chmod(original_mode)
            temporary.replace(path)
        finally:
            temporary.unlink(missing_ok=True)
        thinned.append(str(path.relative_to(app)))
    after_thinning = bundle_bytes(app)

    for path in binaries:
        original_mode = stat.S_IMODE(path.stat().st_mode)
        path.chmod(original_mode | stat.S_IWUSR)
        # Preserve external symbols needed by dyld, plugins and runtime lookup.
        run('/usr/bin/strip', '-S', '-x', str(path))
        path.chmod(original_mode)
        if run('/usr/bin/lipo', '-archs', str(path)).split() != ['arm64']:
            raise ValueError(f'Unexpected architecture after stripping: {path}')
    after_stripping = bundle_bytes(app)

    # Sign every leaf before its enclosing code bundle. The outer app/main is
    # deliberately left to CMake after OCR hashes have captured the final worker.
    signing_targets = set(binaries) - {main}
    for binary in binaries:
        for parent in binary.parents:
            if parent == app:
                break
            if parent.suffix in CODE_BUNDLES:
                signing_targets.add(parent)
    for path in sorted(signing_targets, key=lambda item: (-len(item.parts), str(item))):
        run('/usr/bin/codesign', '--force', '--sign', identity,
            '--preserve-metadata=identifier,entitlements', str(path))

    return dict(schema=1, architecture='arm64',
                sizeScope='uncompressed bundle files; symbolic links excluded',
                machOBinaries=len(binaries),
                thinnedBinaries=thinned, strippedBinaries=len(binaries),
                beforeSlimBytes=before, afterThinningBytes=after_thinning,
                afterStrippingBytes=after_stripping,
                afterNestedSigningBytes=bundle_bytes(app))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', type=Path, required=True)
    parser.add_argument('--staging-root', type=Path, required=True)
    parser.add_argument('--identity', required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    if platform.system() != 'Darwin' or platform.machine() != 'arm64':
        parser.error('Run personal Apple Silicon packaging on a native arm64 Mac')
    if not args.identity:
        parser.error('A signing identity is required')
    try:
        result = slim_bundle(args.app, args.staging_root, args.identity)
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
        print(json.dumps(result))
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'Personal package slimming failed: {error}\n')


if __name__ == '__main__':
    main()
