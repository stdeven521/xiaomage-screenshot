#!/usr/bin/env python3
"""Focused local checks for the pinned OCR patch; no Metal device is required."""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SUBMODULE = 'snow-crates/crates/simd-paddle-ocr-rs'
SOURCE = 'src/runtime/gpu/metal/build.rs'
PATCH = 'cmake/patches/simd-paddle-ocr-metal-sdk-compat.patch'
RUSTC = os.environ.get('RUSTC') or shutil.which('rustc')


@unittest.skipUnless(shutil.which('git'), 'Git is required')
class PersonalMacOSOcrPreparation(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='snow Metal compatibility ')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / SUBMODULE
        self.file = self.source / SOURCE
        self.file.parent.mkdir(parents=True)
        self.git(self.root, 'init', '--quiet')
        self.git(self.source, 'init', '--quiet')
        baseline = self.git(ROOT / SUBMODULE, 'show', 'HEAD:' + SOURCE).stdout
        self.file.write_text(baseline, encoding='utf-8', newline='\n')
        self.git(self.source, 'add', SOURCE)
        self.commit(self.source)
        self.pin = self.git(self.source, 'rev-parse', 'HEAD').stdout.strip()
        self.git(self.root, 'update-index', '--add', '--cacheinfo',
                 '160000,' + self.pin + ',' + SUBMODULE)
        self.commit(self.root)
        patch = self.root / PATCH
        patch.parent.mkdir(parents=True)
        shutil.copyfile(ROOT / PATCH, patch)

    def git(self, directory, *arguments):
        result = subprocess.run(['git', '-C', str(directory), *arguments],
                                capture_output=True, text=True, encoding='utf-8', timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result

    def commit(self, directory):
        self.git(directory, '-c', 'user.name=Fixture', '-c', 'user.email=fixture@example.invalid',
                 '-c', 'commit.gpgsign=false', 'commit', '--quiet', '-m', 'fixture')

    def prepare(self, success=True):
        result = subprocess.run([sys.executable, str(ROOT / 'scripts/prepare-personal-macos-ocr.py'),
                                 '--repo-root', str(self.root)], capture_output=True, text=True,
                                encoding='utf-8', timeout=30)
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        return result

    def test_first_application_and_repeat_preserve_the_submodule_pin(self):
        original = self.file.read_bytes()
        self.assertIn('Applied', self.prepare().stdout)
        patched = self.file.read_bytes()
        self.assertNotEqual(original, patched)
        self.assertIn(b'cargo:rerun-if-env-changed=SDKROOT', patched)
        self.assertIn(b'if sdk_major < 27', patched)
        self.assertLess(patched.index(b'if sdk_major < 27'),
                        patched.index(b'let script = serde_json::to_string'))
        self.assertIn('already applied', self.prepare().stdout)
        self.assertEqual(self.file.read_bytes(), patched)
        self.assertEqual(self.git(self.source, 'rev-parse', 'HEAD').stdout.strip(), self.pin)
        self.assertEqual(self.git(self.root, 'diff', '--cached', '--name-only').stdout, '')

    def test_conflict_refuses_to_modify_any_file(self):
        self.file.write_text(self.file.read_text(encoding='utf-8').replace(
            'let script = serde_json::to_string', 'let conflicting_script = serde_json::to_string'),
            encoding='utf-8', newline='\n')
        conflicted = self.file.read_bytes()
        result = self.prepare(success=False)
        self.assertIn('conflicts', result.stderr)
        self.assertEqual(self.file.read_bytes(), conflicted)

    def test_different_submodule_revision_is_rejected(self):
        (self.source / 'changed.txt').write_text('different revision\n')
        self.git(self.source, 'add', 'changed.txt')
        self.commit(self.source)
        original = self.file.read_bytes()
        self.assertIn('pinned submodule revision', self.prepare(success=False).stderr)
        self.assertEqual(self.file.read_bytes(), original)

    @unittest.skipUnless(RUSTC, 'rustc is required for the extracted Rust semantics check')
    def test_production_filter_omits_only_unavailable_defaults(self):
        self.prepare()
        patched = self.file.read_text(encoding='utf-8')
        # Compile the actual patched Rust block against an object-map fixture;
        # this tests SDK branching without compiling OCR or invoking a GPU.
        start = patched.index('    if sdk_major < 27 {')
        end = patched.index('    let script = serde_json::to_string', start)
        block = patched[start:end]
        harness = r'''
use std::collections::BTreeMap;
#[derive(Clone, Debug, PartialEq)]
struct Pipeline(BTreeMap<&'static str, &'static str>);
impl Pipeline {
    fn as_object_mut(&mut self) -> Option<&mut BTreeMap<&'static str, &'static str>> {
        Some(&mut self.0)
    }
}
fn filter(sdk_major: u32, result: &mut Vec<Pipeline>) {
    let mut native_pipelines = result.clone();
    PRODUCTION_BLOCK
    *result = native_pipelines;
}
fn sample() -> Pipeline {
    Pipeline(BTreeMap::from([
        ("compute_function", "alias:OCR#ocr_General_0"),
        ("threadgroup_size_is_multiple_of_thread_execution_width", "false"),
        ("max_total_threads_per_threadgroup", "0"),
        ("max_call_stack_depth", "1"),
        ("support_indirect_command_buffers", "false"),
        ("support_adding_binary_functions", "false"),
        ("optimize_for_persistent_kernel", "false"),
        ("forward_progress_usage", "Automatic"),
        ("contention_relief", "Automatic"),
    ]))
}
#[test]
fn older_sdk_preserves_all_supported_pipeline_properties() {
    for sdk in [15, 26] {
        let original = sample();
        let mut pipelines = vec![original.clone(), original.clone()];
        filter(sdk, &mut pipelines);
        assert_eq!(pipelines.len(), 2);
        for pipeline in pipelines {
            assert_eq!(pipeline.0.len(), 6);
            for (name, value) in &original.0 {
                if ["optimize_for_persistent_kernel", "forward_progress_usage",
                    "contention_relief"].contains(name) {
                    assert!(!pipeline.0.contains_key(name));
                } else {
                    assert_eq!(pipeline.0.get(name), Some(value));
                }
            }
        }
    }
}
#[test]
fn current_and_future_sdks_keep_every_property() {
    for sdk in [27, 28] {
        let original = vec![sample()];
        let mut pipelines = original.clone();
        filter(sdk, &mut pipelines);
        assert_eq!(pipelines, original);
    }
}
#[test]
fn empty_pipeline_inventory_is_valid() {
    for sdk in [26, 27] {
        let mut pipelines = vec![];
        filter(sdk, &mut pipelines);
        assert!(pipelines.is_empty());
    }
}
'''
        harness = harness.replace('PRODUCTION_BLOCK', block)
        source = self.root / 'metal_sdk_filter.rs'
        source.write_text(harness, encoding='utf-8', newline='\n')
        executable = self.root / ('metal_sdk_filter.exe' if os.name == 'nt' else 'metal_sdk_filter')
        compile_result = subprocess.run([RUSTC, '--edition=2024', '--test', '-Dwarnings',
                                         str(source), '-o', str(executable)],
                                        capture_output=True, text=True, timeout=60)
        self.assertEqual(compile_result.returncode, 0, compile_result.stdout + compile_result.stderr)
        result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('3 passed', result.stdout)


if __name__ == '__main__':
    unittest.main()
