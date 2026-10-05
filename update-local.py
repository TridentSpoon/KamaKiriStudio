#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Explicitly install a newer official KamaKiriStudio release, without root."""
import argparse, hashlib, json, os, pathlib, re, stat, subprocess, sys, tempfile, urllib.request, urllib.parse, zipfile
API='https://api.github.com/repos/TridentSpoon/KamaKiriStudio/releases/latest'
MAX_DOWNLOAD=128*1024*1024
MAX_EXPANDED=256*1024*1024

def version(value):
    if not re.fullmatch(r'\d+\.\d+\.\d+',value):raise ValueError('Unsupported release version.')
    return tuple(map(int,value.split('.')))

class GitHubRedirects(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,req,fp,code,msg,headers,newurl):
        parsed=urllib.parse.urlsplit(newurl)
        if parsed.scheme!='https' or parsed.hostname not in {'github.com','release-assets.githubusercontent.com','objects.githubusercontent.com'}:
            raise ValueError('Unexpected download redirect.')
        return super().redirect_request(req,fp,code,msg,headers,newurl)

def download(url,limit):
    request=urllib.request.Request(url,headers={'User-Agent':'KamaKiriStudio-Updater','Accept':'application/vnd.github+json'})
    with urllib.request.build_opener(GitHubRedirects()).open(request,timeout=20) as response:
        data=response.read(limit+1)
    if len(data)>limit:raise ValueError('Download exceeds the permitted size.')
    return data

def extract_release(archive,destination):
    total=0;seen=set();root=None
    with zipfile.ZipFile(archive) as z:
        if len(z.infolist())>5000:raise ValueError('Too many archive entries.')
        for member in z.infolist():
            path=pathlib.PurePosixPath(member.filename)
            if path.is_absolute() or '..' in path.parts or '\\' in member.filename or not path.parts:raise ValueError('Unsafe archive path.')
            if root is None:root=path.parts[0]
            if path.parts[0]!=root:raise ValueError('Unexpected archive structure.')
            if stat.S_ISLNK(member.external_attr>>16):raise ValueError('Archive symlinks are not permitted.')
            total+=member.file_size
            if total>MAX_EXPANDED:raise ValueError('Expanded archive is too large.')
            if member.is_dir():continue
            if str(path) in seen:raise ValueError('Duplicate archive entry.')
            seen.add(str(path))
            # Downloaded scripts are never extracted or executed.
            relative=pathlib.PurePosixPath(*path.parts[1:])
            if str(relative) not in {'bin/kamakiri-studio','release.json'} and not (len(relative.parts)>2 and relative.parts[0]=='assets' and relative.parts[1] in {'plasmoids','desktoptheme','wallpapers'}):continue
            target=destination.joinpath(*relative.parts);target.parent.mkdir(parents=True,exist_ok=True)
            with z.open(member) as source, target.open('xb') as output:
                while chunk:=source.read(1024*1024):output.write(chunk)
    return destination

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--current',required=True)
    parser.add_argument('--prefix',type=pathlib.Path,default=pathlib.Path.home()/'.local')
    args=parser.parse_args()
    if os.getuid()==0:raise ValueError('Update as your desktop user, without sudo.')
    args.prefix=args.prefix.expanduser().resolve()
    current=version(args.current)
    state=pathlib.Path(os.environ.get('XDG_STATE_HOME',str(pathlib.Path.home()/'.local/state')))/'kamakiri-studio'
    for status in state.glob('*/status.json'):
        if json.loads(status.read_text()).get('state') in {'pending','applying','restoring','recovery-needed'}:raise ValueError('Restore or finish the current appearance trial before updating.')
    receipt=args.prefix/'share/kamakiri-studio/installation.json'
    installed=json.loads(receipt.read_text())
    if installed.get('app')!='KamaKiriStudio' or version(installed['version'])!=current:raise ValueError('The installation receipt does not match the running app.')
    trusted=pathlib.Path(__file__).resolve().parent
    for name in ('update-local.py','install-local.py'):
        path=trusted/name
        if installed.get('files',{}).get(str(path))!=hashlib.sha256(path.read_bytes()).hexdigest():raise ValueError('The installed updater integrity check failed.')
    print('Checking the latest official release…',flush=True)
    release=json.loads(download(API,128*1024))
    tag=release.get('tag_name','');name=tag.removeprefix('v');candidate=version(name)
    if release.get('draft') or release.get('prerelease'):raise ValueError('Only stable releases are supported.')
    if candidate<=current:
        print('You are up to date.' if candidate==current else 'Your development build is newer than the latest official release. No downgrade was installed.')
        return
    filename=f'KamaKiriStudio-{name}.zip'
    asset=next((a for a in release.get('assets',[]) if a.get('name')==filename),None)
    if not asset or not re.fullmatch(r'sha256:[0-9a-f]{64}',asset.get('digest','')):raise ValueError('This release has no supported, verified archive.')
    url=f'https://github.com/TridentSpoon/KamaKiriStudio/releases/download/{tag}/{filename}'
    if asset.get('browser_download_url')!=url or not 0<asset.get('size',0)<=MAX_DOWNLOAD:raise ValueError('Unexpected official release asset.')
    print(f'Downloading and verifying {name}…',flush=True)
    data=download(url,MAX_DOWNLOAD)
    if len(data)!=asset['size'] or hashlib.sha256(data).hexdigest()!=asset['digest'][7:]:raise ValueError('Release archive integrity verification failed.')
    with tempfile.TemporaryDirectory(prefix='kamakiri-update-') as folder:
        folder=pathlib.Path(folder);archive=folder/'release.zip';archive.write_bytes(data)
        bundle=extract_release(archive,folder/'bundle');manifest=json.loads((bundle/'release.json').read_text())
        if manifest.get('app')!='KamaKiriStudio' or manifest.get('version')!=name or manifest.get('status')=='development':raise ValueError('Release manifest does not match the requested update.')
        binary=bundle/'bin/kamakiri-studio'
        if not binary.is_file() or binary.stat().st_size>32*1024*1024 or hashlib.sha256(binary.read_bytes()).hexdigest()!=manifest.get('sha256'):raise ValueError('Release binary integrity verification failed.')
        binary.chmod(0o700)
        probe=subprocess.run([str(binary),'--version'],env=dict(os.environ,QT_QPA_PLATFORM='offscreen'),capture_output=True,text=True,timeout=10)
        if probe.returncode or probe.stdout.strip()!=f'KamaKiriStudio {name}':raise ValueError('This release cannot run on your system; the installed app was not changed.')
        # Use the installed, receipt-verified installer, never the downloaded installer.
        subprocess.run([sys.executable,str(trusted/'install-local.py'),'--bundle',str(bundle),'--prefix',str(args.prefix)],check=True,stdout=subprocess.DEVNULL)
    print(f'Installed {name}. Close and reopen KamaKiriStudio to use the update.')
if __name__=='__main__':
    try:main()
    except Exception as error:print('Update could not finish: '+str(error),file=sys.stderr);sys.exit(1)
