#!/usr/bin/env python3
"""Apply the versioned Metal SDK compatibility patch to the pinned OCR checkout."""

import argparse
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
SUBMODULE = 'snow-crates/crates/simd-paddle-ocr-rs'
PATCH = 'cmake/patches/simd-paddle-ocr-metal-sdk-compat.patch'


def git(directory, *arguments):
    return subprocess.run(['git', '-C', str(directory), *arguments],
                          capture_output=True, text=True, encoding='utf-8', timeout=30)


def checked_git(directory, *arguments):
    result = git(directory, *arguments)
    if result.returncode:
        raise RuntimeError(result.stderr.strip() or 'Unable to inspect the OCR checkout')
    return result.stdout.strip()


def prepare(repo_root):
    repo_root = Path(repo_root).resolve()
    source = repo_root / SUBMODULE
    patch = repo_root / PATCH
    if not patch.is_file():
        raise RuntimeError(f'Missing versioned Metal compatibility patch: {patch}')
    entry = checked_git(repo_root, 'ls-tree', 'HEAD', '--', SUBMODULE).split()
    if len(entry) != 4 or entry[:2] != ['160000', 'commit'] or entry[3] != SUBMODULE:
        raise RuntimeError('The repository must record the pinned OCR Git submodule')
    if not source.is_dir() or not (source / '.git').exists():
        raise RuntimeError('Initialize Git submodules before preparing macOS OCR')
    if Path(checked_git(source, 'rev-parse', '--show-toplevel')).resolve() != source:
        raise RuntimeError('OCR source is not an initialized Git submodule checkout')
    if checked_git(source, 'rev-parse', 'HEAD') != entry[2]:
        raise RuntimeError('OCR checkout does not match the repository-pinned submodule revision')

    forward = git(source, 'apply', '--check', '--whitespace=error-all', str(patch))
    if not forward.returncode:
        checked_git(source, 'apply', '--whitespace=error-all', str(patch))
        print('Applied the macOS Metal SDK compatibility patch; GPU compilation remains enabled.')
        return
    reverse = git(source, 'apply', '--reverse', '--check', str(patch))
    if not reverse.returncode:
        print('The macOS Metal SDK compatibility patch is already applied.')
        return
    raise RuntimeError('The Metal compatibility patch conflicts with the OCR checkout; '
                       'no files were changed.\n' + forward.stderr.strip())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo-root', type=Path, default=ROOT)
    args = parser.parse_args()
    try:
        prepare(args.repo_root)
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f'Unable to prepare macOS OCR: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
