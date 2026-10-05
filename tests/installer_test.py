# SPDX-License-Identifier: MIT
import importlib.util,json,pathlib,tempfile,unittest
from unittest.mock import patch
ROOT=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('installer',ROOT/'install-local.py')
installer=importlib.util.module_from_spec(spec);spec.loader.exec_module(installer)
class InstallerTest(unittest.TestCase):
    def test_failed_receipt_write_restores_previous_installation(self):
        with tempfile.TemporaryDirectory() as folder:
            root=pathlib.Path(folder);bundle=root/'bundle';(bundle/'bin').mkdir(parents=True)
            (bundle/'bin/kamakiri-studio').write_bytes(b'original binary')
            (bundle/'release.json').write_text(json.dumps({'app':'KamaKiriStudio','version':'0.5.0'}))
            for name in ['install-local.py','update-local.py']:(bundle/name).write_text('# trusted helper')
            prefix=root/'prefix';config=root/'config';argv=['installer','--prefix',str(prefix),'--config-root',str(config)]
            with patch.object(installer,'__file__',str(bundle/'install-local.py')),patch('sys.argv',argv),patch.object(installer.shutil,'which',return_value=None),patch('builtins.print'):
                installer.main();receipt=prefix/'share/kamakiri-studio/installation.json';before=receipt.read_bytes()
                (bundle/'bin/kamakiri-studio').write_bytes(b'updated binary')
                real_write=installer.atomically_write
                def fail_receipt(path,data,mode):
                    if path==receipt:raise OSError('simulated receipt failure')
                    real_write(path,data,mode)
                with patch.object(installer,'atomically_write',side_effect=fail_receipt):
                    with self.assertRaises(OSError):installer.main()
                self.assertEqual((prefix/'bin/kamakiri-studio').read_bytes(),b'original binary')
                self.assertEqual(receipt.read_bytes(),before)
if __name__=='__main__':unittest.main()
