# SPDX-License-Identifier: MIT
import importlib.util, io, pathlib, stat, tempfile, unittest, zipfile, json, hashlib
from unittest.mock import patch
ROOT=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('updater',ROOT/'update-local.py')
updater=importlib.util.module_from_spec(spec);spec.loader.exec_module(updater)
class UpdaterTest(unittest.TestCase):
    def archive(self,entries):
        result=io.BytesIO()
        with zipfile.ZipFile(result,'w') as z:
            for name,data in entries:z.writestr(name,data)
        result.seek(0);return result
    def test_extracts_assets_but_never_downloaded_scripts(self):
        with tempfile.TemporaryDirectory() as root:
            dest=pathlib.Path(root)/'bundle'
            updater.extract_release(self.archive([('KamaKiriStudio/bin/kamakiri-studio',b'binary'),('KamaKiriStudio/install-local.py',b'unsafe'),('KamaKiriStudio/assets/wallpapers/a.webp',b'image')]),dest)
            self.assertTrue((dest/'bin/kamakiri-studio').exists());self.assertFalse((dest/'install-local.py').exists());self.assertTrue((dest/'assets/wallpapers/a.webp').exists())
    def test_rejects_traversal_absolute_paths_and_symlinks(self):
        for name in ['../escape','/absolute','KamaKiriStudio/assets/../../escape','KamaKiriStudio\\escape']:
            with self.subTest(name=name),tempfile.TemporaryDirectory() as root:
                with self.assertRaises(ValueError):updater.extract_release(self.archive([(name,b'data')]),pathlib.Path(root))
        entry=zipfile.ZipInfo('KamaKiriStudio/bin/kamakiri-studio');entry.external_attr=(stat.S_IFLNK|0o777)<<16
        with tempfile.TemporaryDirectory() as root:
            with self.assertRaises(ValueError):updater.extract_release(self.archive([(entry,b'/etc/passwd')]),pathlib.Path(root))
    def test_rejects_multiple_roots_and_duplicate_entries(self):
        for entries in [[('One/release.json',b'{}'),('Two/release.json',b'{}')],[('One/release.json',b'{}'),('One/release.json',b'{}')]]:
            with tempfile.TemporaryDirectory() as root:
                with self.assertRaises(ValueError):updater.extract_release(self.archive(entries),pathlib.Path(root))
    def test_corrupt_archive_is_rejected_before_any_installer_runs(self):
        with tempfile.TemporaryDirectory() as folder:
            root=pathlib.Path(folder);trusted=root/'share/kamakiri-studio';trusted.mkdir(parents=True)
            files={}
            for name in ['update-local.py','install-local.py']:
                path=trusted/name;path.write_bytes(b'trusted helper');files[str(path)]=hashlib.sha256(path.read_bytes()).hexdigest()
            (trusted/'installation.json').write_text(json.dumps({'app':'KamaKiriStudio','version':'0.5.0','files':files}))
            release={'tag_name':'v0.6.0','assets':[{'name':'KamaKiriStudio-0.6.0.zip','digest':'sha256:'+'0'*64,'size':3,'browser_download_url':'https://github.com/TridentSpoon/KamaKiriStudio/releases/download/v0.6.0/KamaKiriStudio-0.6.0.zip'}]}
            with patch.object(updater,'__file__',str(trusted/'update-local.py')),patch('sys.argv',['updater','--current','0.5.0','--prefix',str(root)]),patch.dict(updater.os.environ,{'XDG_STATE_HOME':str(root/'state')}),patch.object(updater,'download',side_effect=[json.dumps(release).encode(),b'bad']),patch.object(updater.subprocess,'run') as install,patch('builtins.print'):
                with self.assertRaisesRegex(ValueError,'integrity'):updater.main()
                install.assert_not_called()
    def test_version_comparison_is_numeric_and_strict(self):
        self.assertGreater(updater.version('0.10.0'),updater.version('0.9.0'))
        for value in ['../0.6.0','0.6.0;exec','latest','0.6.0-beta']:
            with self.assertRaises(ValueError):updater.version(value)
if __name__=='__main__':unittest.main()
