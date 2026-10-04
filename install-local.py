#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Install only KamaKiriStudio's own binary and launchers, without root or downloads."""
import argparse, hashlib, json, os, pathlib, shutil, subprocess, tempfile

def sha(data): return hashlib.sha256(data).hexdigest()
def desktop_quote(path):
    value=str(path)
    for char in ('\\','"','`','$'):
        value=value.replace(char,'\\'+char)
    # Desktop-file string unescaping happens before Exec argument parsing.
    return '"'+value.replace('\\','\\\\')+'"'
def atomically_write(path,data,mode):
    descriptor,tmp=tempfile.mkstemp(prefix='.kamakiri-',dir=path.parent)
    try:
        with os.fdopen(descriptor,'wb') as stream:
            stream.write(data);stream.flush();os.fsync(stream.fileno())
        os.chmod(tmp,mode);os.replace(tmp,path)
    finally:
        if os.path.exists(tmp):os.unlink(tmp)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix',type=pathlib.Path,default=pathlib.Path.home()/'.local')
    parser.add_argument('--config-root',type=pathlib.Path,default=pathlib.Path(os.environ.get('XDG_CONFIG_HOME',str(pathlib.Path.home()/'.config'))))
    parser.add_argument('--dry-run',action='store_true')
    args=parser.parse_args()
    if os.getuid()==0:raise SystemExit('Install as your desktop user, without sudo.')
    bundle=pathlib.Path(__file__).resolve().parent
    source=bundle/'bin/kamakiri-studio'
    if not source.is_file():raise SystemExit('Build the binary and place it in bin/kamakiri-studio first.')
    prefix=args.prefix.expanduser().absolute();config=args.config_root.expanduser().absolute()
    binary=prefix/'bin/kamakiri-studio'
    launcher=prefix/'share/applications/kamakiri-studio.desktop'
    startup=config/'autostart/kamakiri-studio-recovery.desktop'
    receipt=prefix/'share/kamakiri-studio/installation.json'
    entries={
        binary:(source.read_bytes(),0o755),
        launcher:((f'[Desktop Entry]\nType=Application\nName=KamaKiriStudio\nComment=KDE appearance trials and desktop session switching\nExec={desktop_quote(binary)}\nIcon=preferences-desktop-theme-global\nTerminal=false\nCategories=Settings;DesktopSettings;Qt;KDE;\n').encode(),0o644),
        startup:((f'[Desktop Entry]\nType=Application\nName=KamaKiriStudio recovery\nComment=Restore abandoned appearance trials at KDE login\nExec={desktop_quote(binary)} --recover-all\nIcon=preferences-desktop-theme-global\nTerminal=false\nNoDisplay=true\nOnlyShowIn=KDE;\n').encode(),0o644),
    }
    for name,mode,icon in [('KamaKiri Launcher','launcher','view-app-grid'),('KamaKiri Dashboard','dashboard','dashboard-show')]:
        target=prefix/'share/applications'/f'kamakiri-{mode}.desktop'
        entries[target]=((f'[Desktop Entry]\nType=Application\nName={name}\nComment=Rounded KDE desktop popup\nExec={desktop_quote(binary)} --{mode}\nIcon={icon}\nTerminal=false\nCategories=Utility;Qt;KDE;\n').encode(),0o644)
    previous={}
    if receipt.exists():
        manifest=json.loads(receipt.read_text())
        if manifest.get('app')!='KamaKiriStudio':raise SystemExit('Installation receipt belongs to a different application.')
        previous=manifest.get('files',{})
    for target in entries:
        if target.is_symlink():raise SystemExit(f'Refusing to replace a symlink: {target}')
        if target.exists() and (str(target) not in previous or sha(target.read_bytes())!=previous[str(target)]):
            raise SystemExit(f'Existing file is not an unmodified KamaKiriStudio installation: {target}')
    print('KamaKiriStudio local installation:')
    for target in entries:print(' ',target)
    print(' No desktop appearance changes, dependency installs, or downloads.')
    if args.dry_run:return
    for target in [*entries,receipt]:
        target.parent.mkdir(parents=True,exist_ok=True)
        if target.parent.is_symlink() or target.parent.stat().st_uid!=os.getuid():raise SystemExit(f'Destination directory must belong to you and not be a symlink: {target.parent}')
    for target,(data,mode) in entries.items():atomically_write(target,data,mode)
    manifest={'app':'KamaKiriStudio','version':'0.4.1','files':{str(target):sha(data) for target,(data,_) in entries.items()}}
    atomically_write(receipt,(json.dumps(manifest,indent=2)+'\n').encode(),0o600)
    updater=shutil.which('update-desktop-database')
    if updater:subprocess.run([updater,str(launcher.parent)],check=False)
    print('Installed. Open KamaKiriStudio from the Applications menu.')
if __name__=='__main__':main()
