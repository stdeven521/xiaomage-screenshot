#!/usr/bin/env python3
"""Check Windows overlay CRT configure arguments without downloading or building ports."""

import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
OPTIONS = None


class WindowsPortCrtProfiles(unittest.TestCase):
    def test_static_libraries_preserve_both_triplet_crt_profiles(self):
        cmake = OPTIONS.cmake or shutil.which('cmake')
        self.assertTrue(cmake, 'CMake is required for the port configuration fixture')
        vcpkg = Path(OPTIONS.vcpkg_root).resolve()
        self.assertTrue((vcpkg / 'ports/crashpad/portfile.cmake').is_file(),
                        'The pinned repository-managed vcpkg tree is required')
        for port in ('opencv4', 'minizip-ng', 'crashpad'):
            for architecture in ('x64', 'arm64'):
                for crt in ('dynamic', 'static'):
                    with self.subTest(port=port, architecture=architecture, crt=crt):
                        with tempfile.TemporaryDirectory(prefix='snow-crt-profile-') as directory:
                            arguments = [cmake, f'-DREPO_ROOT={ROOT.as_posix()}',
                                         f'-DVCPKG_ROOT_DIR={vcpkg.as_posix()}',
                                         f'-DFIXTURE_DIR={Path(directory).as_posix()}',
                                         f'-DPROFILE_PORT={port}', f'-DPROFILE_CRT={crt}',
                                         f'-DPROFILE_ARCH={architecture}']
                            if OPTIONS.overlay_root:
                                arguments.append(
                                    f'-DOVERLAY_ROOT={Path(OPTIONS.overlay_root).resolve().as_posix()}')
                            arguments += ['-P', str(ROOT / 'cmake/test-support/vcpkg-crt-profile.cmake')]
                            result = subprocess.run(arguments, text=True, capture_output=True,
                                                    timeout=30)
                            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--cmake')
    parser.add_argument('--vcpkg-root', default=ROOT / '.tools/vcpkg')
    parser.add_argument('--overlay-root')
    OPTIONS, remaining = parser.parse_known_args()
    unittest.main(argv=[sys.argv[0], *remaining])
