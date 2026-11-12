#!/usr/bin/env python3
"""Focused personal-package staging, architecture and deployment-order checks."""

import importlib.util
import hashlib
import os
from pathlib import Path
import platform
import plistlib
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    'personal_package', ROOT / 'scripts/slim-personal-macos-bundle.py')
package = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(package)
THIN = bytes.fromhex('cffaedfe')
FAT = bytes.fromhex('cafebabe')


class PersonalPackageStaging(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='personal package ')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.stage = self.root / '_CPack_Packages'
        self.app = self.stage / 'Darwin/DragNDrop/personal/snow_shot.app'
        self.main = self.binary('Contents/MacOS/snow_shot')
        self.worker = self.binary('Contents/MacOS/snow-ocr-process')
        self.framework = self.app / 'Contents/Frameworks/QtCore.framework'
        self.qt = self.binary('Contents/Frameworks/QtCore.framework/Versions/A/QtCore', fat=True)
        self.helper = self.binary(
            'Contents/Frameworks/QtCore.framework/Helpers/Nested.app/Contents/MacOS/helper')
        self.plugin = self.binary('Contents/PlugIns/platforms/libqcocoa.dylib', fat=True)
        (self.app / 'Contents/Info.plist').write_bytes(
            plistlib.dumps({'CFBundleExecutable': 'snow_shot'}))
        self.model = self.app / 'Contents/Resources/assets/ocr/model.onnx'
        self.model.parent.mkdir(parents=True)
        self.model.write_bytes(b'preserved OCR model')
        self.original = self.root / 'build-symbols/snow-ocr-process.dSYM'
        self.original.parent.mkdir()
        self.original.write_bytes(b'preserved original debug information')
        self.calls = []
        self.failure = None

    def binary(self, relative, fat=False):
        path = self.app / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes((FAT if fat else THIN) + b'x' * (196 if fat else 96))
        path.chmod(0o755)
        return path

    def tool(self, *command):
        self.calls.append(command)
        name = Path(command[0]).name
        if name == 'lipo' and command[1] == '-archs':
            path = Path(command[2])
            if path.name == 'intel-only':
                return 'x86_64'
            return 'arm64 x86_64' if path.read_bytes()[:4] == FAT else 'arm64'
        operation = 'thin' if name == 'lipo' else name
        if self.failure == operation:
            raise subprocess.CalledProcessError(37, command)
        if name == 'lipo':
            self.assertEqual(command[2:5], ('-thin', 'arm64', '-output'))
            Path(command[5]).write_bytes(THIN + Path(command[1]).read_bytes()[4:100])
        elif name == 'strip':
            self.assertEqual(command[1:3], ('-S', '-x'))
            path = Path(command[-1])
            path.write_bytes(path.read_bytes()[:-16])
        elif name == 'codesign':
            self.assertNotIn('--deep', command)
        else:
            self.fail(f'Unexpected external command: {command}')
        return ''

    def slim(self):
        with patch.object(package, 'run', side_effect=self.tool):
            return package.slim_bundle(self.app, self.stage, '-')

    def test_thins_only_fat_files_preserves_resources_and_signs_inside_out(self):
        result = self.slim()
        self.assertEqual(result['thinnedBinaries'],
                         [str(self.qt.relative_to(self.app)), str(self.plugin.relative_to(self.app))])
        self.assertEqual(result['strippedBinaries'], 5)
        self.assertGreater(result['beforeSlimBytes'], result['afterThinningBytes'])
        self.assertGreater(result['afterThinningBytes'], result['afterStrippingBytes'])
        self.assertEqual(result['afterNestedSigningBytes'], package.bundle_bytes(self.app))
        self.assertEqual(self.model.read_bytes(), b'preserved OCR model')
        self.assertEqual(self.original.read_bytes(), b'preserved original debug information')
        signs = [index for index, call in enumerate(self.calls) if Path(call[0]).name == 'codesign']
        strips = [index for index, call in enumerate(self.calls) if Path(call[0]).name == 'strip']
        self.assertLess(max(strips), min(signs))
        signed = [Path(self.calls[index][-1]) for index in signs]
        self.assertNotIn(self.main, signed)
        self.assertNotIn(self.app, signed)
        nested = self.helper.parents[2]
        self.assertLess(signed.index(self.helper), signed.index(nested))
        self.assertLess(signed.index(nested), signed.index(self.framework))
        self.assertLess(signed.index(self.qt), signed.index(self.framework))

    def test_symlinks_are_neither_counted_nor_processed(self):
        before = package.bundle_bytes(self.app)
        links = [('Contents/MacOS/worker-link', self.worker, False),
                 ('Contents/MacOS/assets', self.model.parents[1], True),
                 ('Contents/Resources/external', self.original.parent, True)]
        try:
            for relative, target, directory in links:
                (self.app / relative).symlink_to(target, target_is_directory=directory)
        except OSError as error:
            self.skipTest(f'Symlink creation is unavailable: {error}')
        result = self.slim()
        self.assertEqual(result['beforeSlimBytes'], before)
        self.assertEqual(result['machOBinaries'], 5)
        self.assertFalse(any('worker-link' in str(call) or 'external' in str(call)
                             for call in self.calls))

    def test_non_staging_app_is_rejected_without_any_tool_call(self):
        original = self.root / 'snow_shot.app'
        self.app.rename(original)
        self.app = original
        with self.assertRaisesRegex(ValueError, 'CPack staging'):
            self.slim()
        self.assertEqual(self.calls, [])

    def test_missing_arm64_is_rejected_before_mutating_any_binary(self):
        self.binary('Contents/MacOS/intel-only')
        original = self.qt.read_bytes()
        with self.assertRaisesRegex(ValueError, 'no arm64 slice'):
            self.slim()
        self.assertEqual(self.qt.read_bytes(), original)
        self.assertTrue(all(call[1] == '-archs' for call in self.calls))

    def test_thin_strip_and_sign_failures_propagate(self):
        for failure in ('thin', 'strip', 'codesign'):
            with self.subTest(failure=failure):
                self.failure = failure
                self.calls = []
                with self.assertRaises(subprocess.CalledProcessError):
                    self.slim()
                if failure in ('thin', 'strip'):
                    self.assertFalse(any(Path(call[0]).name == 'codesign' for call in self.calls))
        self.assertFalse(list(self.app.rglob('*.thin-*')))


@unittest.skipUnless(shutil.which('cmake'), 'CMake is required for deployment orchestration')
class PersonalPackageDeployment(unittest.TestCase):
    def deploy(self, enabled=True, arch='arm64', config='Release', fail=False):
        with tempfile.TemporaryDirectory(prefix='personal deployment ') as temporary:
            root = Path(temporary)
            runtime = root / 'snow_shot.app/Contents/MacOS'
            runtime.mkdir(parents=True)
            (runtime / 'snow-shot-mcp').write_text('fixture')
            log = root / 'calls.txt'
            # Exercise CMake branching and argument order without native macOS tools.
            prefix = f'''
function(execute_process)
    file(APPEND "{log.as_posix()}" "${{ARGV}}\n")
    if("${{ARGV}}" MATCHES "slim-personal-macos-bundle.py" AND "$ENV{{FAIL_SLIM}}" STREQUAL "1")
        message(FATAL_ERROR "fixture slimming failure")
    endif()
    set(_result 0 PARENT_SCOPE)
    set(_output "" PARENT_SCOPE)
    set(_error "" PARENT_SCOPE)
endfunction()
'''
            script = (ROOT / 'cmake/DeploySnowShotMacOS.cmake.in').read_text()
            values = dict(SNOW_SHOT_ENABLE_MCP='ON', SNOW_MACOS_CODESIGN_IDENTITY='-',
                          SNOW_MACOS_OCR_ARCH=arch, SNOW_SHOT_QT_STATIC='OFF',
                          SNOW_MACOS_OCR_RUNTIME_ONLY='OFF', SNOW_SHOT_RELEASE_STATIC='OFF',
                          SNOW_MACOS_OCR_ASSETS_ENABLED='ON', Python3_EXECUTABLE='python3',
                          SNOW_MACOS_OCR_TOOL='/source/scripts/snow-shot-macos-ocr.py',
                          SNOW_MACOS_OCR_MANIFEST='/source/manifest.json',
                          SNOW_FFMPEG_ROOT='/vcpkg', SNOW_MACDEPLOYQT='macdeployqt',
                          CMAKE_BINARY_DIR=root.as_posix())
            for key, value in values.items():
                script = script.replace('@' + key + '@', value)
            path = root / 'deploy.cmake'
            path.write_text(prefix + script)
            environment = dict(os.environ, SNOW_SHOT_PERSONAL_ARM64_PACKAGE=str(int(enabled)),
                               FAIL_SLIM=str(int(fail)))
            result = subprocess.run(['cmake', '-DCMAKE_INSTALL_PREFIX=' + root.as_posix(),
                                     '-DCMAKE_INSTALL_CONFIG_NAME=' + config, '-P', str(path)],
                                    env=environment, capture_output=True, text=True)
            calls = [line.split(';') for line in log.read_text().splitlines()] if log.exists() else []
            return calls, result

    def test_personal_arm64_slims_before_ocr_hashes_outer_sign_and_verify(self):
        calls, result = self.deploy()
        self.assertEqual(result.returncode, 0, result.stderr)
        deploy = next(call for call in calls if 'macdeployqt' in call)
        self.assertIn('-no-codesign', deploy)
        self.assertFalse(any(value.startswith('-codesign=') for value in deploy))
        slim = next(index for index, call in enumerate(calls)
                    if any(value.endswith('/slim-personal-macos-bundle.py') for value in call))
        finalize = next(index for index, call in enumerate(calls) if 'finalize' in call)
        sign = next(index for index, call in enumerate(calls) if '--sign' in call)
        verify = next(index for index, call in enumerate(calls) if 'verify' in call)
        self.assertLess(calls.index(deploy), slim)
        self.assertLess(slim, finalize)
        self.assertLess(finalize, sign)
        self.assertLess(sign, verify)
        self.assertIn('COMMAND_ERROR_IS_FATAL', calls[slim])

    def test_normal_arm64_and_intel_deployment_do_not_slim(self):
        for arch in ('arm64', 'x64'):
            calls, result = self.deploy(enabled=False, arch=arch)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertFalse(any('slim-personal-macos-bundle.py' in str(call) for call in calls))
            self.assertIn('-codesign=-', next(call for call in calls if 'macdeployqt' in call))

    def test_personal_intel_or_debug_requests_stop_before_deployment(self):
        for arch, config in (('x64', 'Release'), ('arm64', 'Debug')):
            calls, result = self.deploy(arch=arch, config=config)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(calls, [])

    def test_slimming_failure_prevents_hashing_signing_and_verification(self):
        calls, result = self.deploy(fail=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(any('finalize' in call or '--sign' in call or 'verify' in call
                             for call in calls))


@unittest.skipUnless(platform.system() == 'Darwin' and platform.machine() == 'arm64',
                     'Native Apple Silicon tools are required')
class PersonalPackageNative(unittest.TestCase):
    def test_thin_framework_exports_signatures_and_loadable_application(self):
        """Use tiny native binaries, without Qt, GPU execution, models or network."""
        with tempfile.TemporaryDirectory(prefix='native personal package ') as temporary:
            root = Path(temporary)
            stage = root / '_CPack_Packages'
            app = stage / 'native/snow_shot.app'
            runtime = app / 'Contents/MacOS'
            runtime.mkdir(parents=True)
            framework = app / 'Contents/Frameworks/Fixture.framework'
            version = framework / 'Versions/A'
            (version / 'Resources').mkdir(parents=True)
            (framework / 'Versions/Current').symlink_to('A')
            (framework / 'Fixture').symlink_to('Versions/Current/Fixture')
            (framework / 'Resources').symlink_to('Versions/Current/Resources')
            for destination, name, kind in ((app / 'Contents', 'snow_shot', 'APPL'),
                                             (version / 'Resources', 'Fixture', 'FMWK')):
                (destination / 'Info.plist').write_bytes(plistlib.dumps({
                    'CFBundleExecutable': name, 'CFBundleIdentifier': 'example.personal.' + name,
                    'CFBundlePackageType': kind, 'CFBundleVersion': '1.0',
                }))
            library_source = root / 'library.c'
            library_source.write_text('int fixture_value(int value) { return value + 1; }\n')
            library = version / 'Fixture'
            package.run('xcrun', 'clang', '-arch', 'arm64', '-arch', 'x86_64',
                        '-g', '-O0', '-dynamiclib', str(library_source), '-o', str(library),
                        '-Wl,-install_name,@rpath/Fixture.framework/Versions/A/Fixture')
            source = root / 'main.c'
            source.write_text('#include <dlfcn.h>\nint main(void) {\n'
                              'void *handle = dlopen("../Frameworks/Fixture.framework/Fixture", RTLD_NOW);\n'
                              'if (!handle) return 1;\n'
                              'int (*value)(int) = (int (*)(int))dlsym(handle, "fixture_value");\n'
                              'return value && value(41) == 42 ? 0 : 2;\n}\n')
            original = root / 'original'
            package.run('xcrun', 'clang', '-arch', 'arm64', '-g', '-O0', str(source),
                        '-o', str(original))
            symbols = root / 'original.dSYM'
            package.run('xcrun', 'dsymutil', str(original), '-o', str(symbols))
            original_hash = hashlib.sha256(original.read_bytes()).hexdigest()
            original_uuid = package.run('xcrun', 'dwarfdump', '--uuid', str(original)).split()[1]
            original_exports = package.run('/usr/bin/nm', '-arch', 'arm64', '-gUj', str(library))
            for name in ('snow_shot', 'snow-ocr-process'):
                shutil.copy2(original, runtime / name)
            result = package.slim_bundle(app, stage, '-')
            # Production finalizes OCR resources here, then signs the outer app.
            resources = app / 'Contents/Resources'
            resources.mkdir()
            (resources / 'runtime-manifest.json').write_text('{}\n')
            package.run('/usr/bin/codesign', '--force', '--sign', '-', str(app))
            package.run('/usr/bin/codesign', '--verify', '--deep', '--strict', str(app))
            subprocess.run([str(runtime / 'snow_shot')], cwd=runtime, check=True)
            self.assertEqual(package.run('/usr/bin/lipo', '-archs', str(library)), 'arm64')
            self.assertEqual(package.run('/usr/bin/nm', '-arch', 'arm64', '-gUj', str(library)),
                             original_exports)
            self.assertEqual(package.run('xcrun', 'dwarfdump', '--uuid',
                                         str(runtime / 'snow_shot')).split()[1], original_uuid)
            self.assertEqual(package.run('xcrun', 'dwarfdump', '--uuid', str(symbols)).split()[1],
                             original_uuid)
            self.assertEqual(hashlib.sha256(original.read_bytes()).hexdigest(), original_hash)
            self.assertGreater(result['beforeSlimBytes'], result['afterStrippingBytes'])


if __name__ == '__main__':
    unittest.main()
