# SPDX-License-Identifier: MIT
"""Isolated import probes and recovery failure preserve user configuration."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import uuid

BINARY = str(Path(sys.argv.pop(1)).resolve())

class PluginCompatibilityTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        self.environment = dict(os.environ, QT_QPA_PLATFORM='offscreen', QML_DISABLE_DISK_CACHE='1',
                                XDG_STATE_HOME=str(self.root / 'state'),
                                XDG_CONFIG_HOME=str(self.root / 'config'),
                                XDG_CACHE_HOME=str(self.root / 'cache'),
                                XDG_DATA_HOME=str(self.root / 'data'),
                                DBUS_SESSION_BUS_ADDRESS='unix:path=' + str(self.root / 'missing-session-bus'))

    def tearDown(self):
        self.directory.cleanup()

    def run_app(self, *arguments):
        return subprocess.run([BINARY, *arguments], env=self.environment,
                              capture_output=True, text=True, timeout=20)

    def make_plugin(self, name, source, metadata=None):
        package = self.root / 'data/plasma/plasmoids' / name
        (package / 'contents/ui').mkdir(parents=True)
        (package / 'contents/ui/main.qml').write_text(source)
        (package / 'metadata.json').write_text(json.dumps(metadata or {}))
        return package

    def test_probe_imports_without_instantiating_widget(self):
        self.make_plugin('org.kamakiri.testprobe',
                         'import QtQml\nQtObject { property int runtimeOnly: functionThatMustNotRun() }\n')
        result = self.run_app('--probe-plugin', 'org.kamakiri.testprobe')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('KAMAKIRI_PLUGIN_IMPORT_OK', result.stdout)
        self.assertNotIn("ReferenceError", result.stderr)

    def test_incompatible_import_has_named_error_and_no_success(self):
        self.make_plugin('org.kamakiri.testbroken', 'import Missing.Qt.Plugin 99.0\nQtObject {}\n')
        result = self.run_app('--probe-plugin', 'org.kamakiri.testbroken')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('org.kamakiri.testbroken', result.stderr)
        self.assertNotIn('KAMAKIRI_PLUGIN_IMPORT_OK', result.stdout)

    def test_inherited_package_is_checked_and_cycles_are_rejected(self):
        self.make_plugin('org.kamakiri.testbase', 'import QtQml\nQtObject {}\n')
        self.make_plugin('org.kamakiri.testvariant', 'invalid', {'X-Plasma-RootPath': 'org.kamakiri.testbase'})
        result = self.run_app('--probe-plugin', 'org.kamakiri.testvariant')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.make_plugin('org.kamakiri.testcycle', 'invalid', {'X-Plasma-RootPath': 'org.kamakiri.testcycle'})
        result = self.run_app('--probe-plugin', 'org.kamakiri.testcycle')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('cyclic', result.stderr)

    def test_unverified_recovery_preserves_snapshot_and_current_settings(self):
        trial = self.root / 'state/kamakiri-studio' / str(uuid.uuid4())
        trial.mkdir(parents=True, mode=0o700)
        configuration = self.root / 'config/kdeglobals'
        configuration.parent.mkdir(parents=True)
        configuration.write_text('[General]\nColorScheme=Current\n')
        snapshot = {'changeColors': True, 'desktopLookBefore': {'panel': {'widgets': [{'id': 1, 'type': 'org.kde.plasma.icon'}]}}}
        snapshot_file = trial / 'snapshot.json'
        snapshot_file.write_text(json.dumps(snapshot))
        before = snapshot_file.read_bytes()
        (trial / 'colors-before.ini').write_text('[General]\nColorScheme=Original\n')
        (trial / 'status.json').write_text('{"state":"pending"}')
        (trial / 'request.json').write_text('{"demo":false}')
        result = self.run_app('--recover-all')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Plugin compatibility check blocked', result.stderr)
        self.assertIn('restart', result.stderr)
        self.assertIn('not system packages', result.stderr)
        self.assertEqual(json.loads((trial / 'status.json').read_text())['state'], 'recovery-needed')
        self.assertEqual(snapshot_file.read_bytes(), before)
        self.assertIn('ColorScheme=Current', configuration.read_text())
        self.assertTrue((trial / 'colors-before.ini').exists())

if __name__ == '__main__':
    unittest.main()
