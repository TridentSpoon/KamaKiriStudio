# SPDX-License-Identifier: MIT
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('guided_installer', ROOT / 'install-easy.py')
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)

class GuidedInstallerTest(unittest.TestCase):
    def test_distribution_derivatives(self):
        with tempfile.TemporaryDirectory() as directory:
            release = Path(directory) / 'os-release'
            for name, expected in {'ubuntu':'deb', 'debian':'deb', 'fedora':'rpm', 'rhel':'rpm', 'rocky':'rpm', 'cachyos':'arch', 'arch':'arch'}.items():
                release.write_text('ID=' + name + '\n')
                self.assertEqual(installer.detect_family(release), expected)
            release.write_text('ID=custom\nID_LIKE="ubuntu debian"\n')
            self.assertEqual(installer.detect_family(release), 'deb')
            release.write_text('ID=unknown\n')
            with self.assertRaisesRegex(ValueError, 'not supported'):
                installer.detect_family(release)

    def test_plasma5_rejected(self):
        with patch.object(installer.shutil, 'which', return_value='/usr/bin/plasmashell'), patch.object(installer, 'run', return_value=subprocess.CompletedProcess([], 0, 'plasmashell 5.27.12\n', '')):
            with self.assertRaisesRegex(ValueError, 'Plasma 6 is required'):
                installer.check_desktop()

    def test_missing_plasma_never_installs_desktop(self):
        with patch.object(installer.shutil, 'which', return_value=None), patch.object(installer, 'run') as command:
            with self.assertRaisesRegex(ValueError, 'does not install or replace'):
                installer.check_desktop()
            command.assert_not_called()

    def test_version_probe_uses_headless_minimal_backend(self):
        with patch.object(installer.shutil, 'which', return_value='/usr/bin/tool'), patch.object(installer, 'run', return_value=subprocess.CompletedProcess([], 0, 'plasmashell 6.7.5\n', '')) as execute:
            self.assertEqual(installer.check_desktop(), 'plasmashell 6.7')
            self.assertEqual(execute.call_args.kwargs['env']['QT_QPA_PLATFORM'], 'minimal')
            self.assertEqual(execute.call_args.args[0][-1], '--version')

    def test_missing_color_tool(self):
        with patch.object(installer.shutil, 'which', side_effect=['/usr/bin/plasmashell', None]), patch.object(installer, 'run', return_value=subprocess.CompletedProcess([], 0, 'plasmashell 6.3.6\n', '')):
            with self.assertRaisesRegex(ValueError, 'color-scheme tool'):
                installer.check_desktop()

    def test_debian_missing_candidate(self):
        output = ''.join(name + ':\n  Candidate: ' + ('(none)' if name == 'libkf6kio-dev' else '6.13') + '\n' for name in installer.PACKAGES['deb'])
        with patch.object(installer, 'run', return_value=subprocess.CompletedProcess([], 0, output, '')):
            with self.assertRaisesRegex(ValueError, 'libkf6kio-dev'):
                installer.check_packages('deb')

    def test_rpm_available_and_missing_packages(self):
        output = '\n'.join(name + '.x86_64 6.13 fedora' for name in installer.PACKAGES['rpm'])
        with patch.object(installer.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0, output, '')):
            installer.check_packages('rpm')
        with patch.object(installer.subprocess, 'run', return_value=subprocess.CompletedProcess([], 1, '', 'No matching packages')):
            with self.assertRaisesRegex(ValueError, 'No repositories were added'):
                installer.check_packages('rpm')

    def test_arch_full_upgrade_preserves_prompt(self):
        command = installer.package_commands('arch')[0]
        self.assertIn('-Syu', command)
        self.assertNotIn('--noconfirm', command)
        self.assertIn('--noconfirm', installer.package_commands('arch', True)[0])

    def test_failed_probe_preserves_existing_installation(self):
        def command(argv, **kwargs):
            return subprocess.CompletedProcess(argv, 0, 'wrong version\n' if '--version' in argv else '', '')
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / 'prefix'
            binary = prefix / 'bin/kamakiri-studio'
            binary.parent.mkdir(parents=True)
            binary.write_bytes(b'existing installation')
            with patch.object(installer, 'run', side_effect=command) as execute:
                with self.assertRaisesRegex(ValueError, 'version check'):
                    installer.build_and_install(prefix, Path(directory) / 'config', 2)
                self.assertFalse(any('install-local.py' in str(call.args[0]) for call in execute.call_args_list))
            self.assertEqual(binary.read_bytes(), b'existing installation')

    def test_dry_run_has_no_side_effects(self):
        with patch('sys.argv', ['installer', '--dry-run']), patch.object(installer, 'detect_family', return_value='rpm'), patch.object(installer, 'check_desktop') as desktop, patch.object(installer, 'run') as execute, patch('builtins.print'):
            installer.main()
            desktop.assert_not_called()
            execute.assert_not_called()

    def test_shell_bootstrap_dry_run_never_runs_manager(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            (folder / 'dirname').symlink_to('/usr/bin/dirname')
            marker = folder / 'package-manager-was-run'
            manager = folder / 'apt-get'
            manager.write_text('#!/bin/bash\ntouch "' + str(marker) + '"\nexit 99\n')
            manager.chmod(0o755)
            result = subprocess.run(['/bin/bash', str(ROOT / 'install.sh'), '--dry-run'], env=dict(os.environ, PATH=str(folder)), capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('Python 3 is needed', result.stdout)
            self.assertFalse(marker.exists())

if __name__ == '__main__':
    unittest.main()
