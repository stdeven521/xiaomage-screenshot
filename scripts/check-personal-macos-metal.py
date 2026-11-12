#!/usr/bin/env python3
"""Check OCR's real Metal build module before the expensive application build.

Compile synthetic production/test entries through the pinned build module. This
checks compiler/schema compatibility and archive generation, not GPU execution
or the application's complete neural shader catalog.
"""

import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile
import tomllib


ROOT = Path(__file__).resolve().parents[1]
METAL = ROOT / 'snow-crates/crates/simd-paddle-ocr-rs/src/runtime/gpu/metal'
NEW_PROPERTIES = ('contention_relief', 'forward_progress_usage',
                  'optimize_for_persistent_kernel')


def check():
    if platform.system() != 'Darwin' or platform.machine() != 'arm64':
        raise RuntimeError('This build-tool check requires native Apple Silicon macOS')
    report_dir = ROOT / '.tools/personal-metal-check'
    report_dir.mkdir(parents=True, exist_ok=True)
    (report_dir / 'result.json').unlink(missing_ok=True)
    packages = tomllib.loads((ROOT / 'snow-crates/Cargo.lock').read_text())['package']
    versions = {item['name']: item['version'] for item in packages
                if item['name'] in ('serde_json', 'miniz_oxide')}
    with tempfile.TemporaryDirectory(prefix='fixture-', dir=report_dir) as directory:
        fixture = Path(directory)
        module = fixture / 'src/metal'
        module.mkdir(parents=True)
        shutil.copy2(METAL / 'build.rs', module / 'build.rs')
        shutil.copytree(METAL / 'shaders', module / 'shaders')
        catalog = (METAL / 'catalog.rs').read_text()
        prefix = catalog[:catalog.index('macro_rules!')]
        suffix = catalog[catalog.index('pub fn source('):]
        entries = []
        for name, is_test in (('ProductionProbe', False), ('TestProbe', True)):
            entries.append(
                'Entry { id: "' + name + '", entry: "probe", '
                'body: "kernel void probe(device uint* x [[buffer(0)]], '
                'constant Params& p [[buffer(1)]]) { x[0] += 1u; }", '
                'defines: "", mask: 0, threads: 1, reads: &[false], '
                'neural: false, test: ' + str(is_test).lower() + ' },')
        (module / 'catalog.rs').write_text(
            prefix + 'pub const ENTRIES: &[Entry] = &[\n' + '\n'.join(entries) +
            '\n];\n' + suffix)
        (fixture / 'src/main.rs').write_text(
            '#[path = "metal/build.rs"] mod metal;\nfn main() { metal::build(); }\n')
        (fixture / 'Cargo.toml').write_text(
            '[package]\nname = "personal-metal-build-check"\nversion = "0.0.0"\n'
            'edition = "2024"\n[dependencies]\n' + ''.join(
                f'{name} = "={version}"\n' for name, version in sorted(versions.items())) +
            '[workspace]\n')
        shutil.copy2(ROOT / 'snow-crates/Cargo.lock', fixture / 'Cargo.lock')
        output = fixture / 'out'
        output.mkdir()
        try:
            result = subprocess.run(
                ['cargo', 'run', '--release', '--manifest-path', str(fixture / 'Cargo.toml'),
                 '--target-dir', str(report_dir / 'target')],
                cwd=ROOT, env=dict(os.environ, OUT_DIR=str(output)), text=True,
                capture_output=True, timeout=240)
        except subprocess.TimeoutExpired as error:
            for name, value in (('stdout.log', error.stdout), ('stderr.log', error.stderr)):
                content = value.decode(errors='replace') if isinstance(value, bytes) else value or ''
                (report_dir / name).write_text(content)
            raise RuntimeError('Metal compiler check timed out; partial logs were preserved') from error
        (report_dir / 'stdout.log').write_text(result.stdout)
        (report_dir / 'stderr.log').write_text(result.stderr)
        print(result.stdout, end='')
        print(result.stderr, end='')
        if result.returncode:
            raise RuntimeError('The production Metal build module failed its compiler check')
        sdk = subprocess.check_output(
            ['xcrun', '--sdk', 'macosx', '--show-sdk-version'], text=True).strip()
        script = json.loads((output / 'ocr.mtlp-json').read_text())
        pipelines = script['pipelines']['compute_pipelines']
        if len(pipelines) != 1 or pipelines[0]['compute_function'] != 'alias:OCR#ocr_ProductionProbe_0':
            raise RuntimeError('The native archive must include only the production shader')
        pipeline = pipelines[0]
        expected_new = {'contention_relief': 'Automatic', 'forward_progress_usage': 'Automatic',
                        'optimize_for_persistent_kernel': False}
        if int(sdk.split('.')[0]) < 27:
            if any(name in pipeline for name in NEW_PROPERTIES):
                raise RuntimeError('Metal 26 must not receive Metal 27 pipeline properties')
        elif any(pipeline.get(name) != value for name, value in expected_new.items()):
            raise RuntimeError('Metal 27 pipeline defaults must remain explicit')
        sizes = {}
        for name in ('ocr.metallib', 'ocr.tests.metallib', 'ocr.binary.metallib'):
            sizes[name] = (output / name).stat().st_size
            if not sizes[name] or not (output / (name + '.deflate')).stat().st_size:
                raise RuntimeError(f'Missing Metal library: {name}')
        (report_dir / 'result.json').write_text(json.dumps(
            {'sdk': sdk, 'pipeline': pipeline, 'libraryBytes': sizes,
             'compilerCompatibility': 'passed', 'gpuExecution': 'not tested'}, indent=2) + '\n')
        print(f'Production Metal build module passed for SDK {sdk}; GPU execution is not tested.')


if __name__ == '__main__':
    check()
