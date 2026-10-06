#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Install KamaKiriStudio on an existing KDE Plasma 6 desktop."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent
PACKAGES = {
    'deb': ['build-essential', 'cmake', 'ninja-build', 'pkg-config',
            'qt6-base-dev', 'qt6-declarative-dev', 'qt6-image-formats-plugins',
            'libkf6config-dev', 'libkf6idletime-dev', 'libkf6service-dev',
            'libkf6kio-dev', 'libkf6windowsystem-dev', 'libwayland-dev',
            'libwayland-bin', 'qml6-module-qtquick', 'qml6-module-qtquick-controls',
            'qml6-module-qtquick-layouts', 'qml6-module-qtquick-window',
            'qml6-module-qtqml-workerscript', 'breeze'],
    'rpm': ['gcc', 'gcc-c++', 'cmake', 'ninja-build', 'pkgconf-pkg-config',
            'qt6-qtbase-devel', 'qt6-qtdeclarative-devel', 'qt6-qtimageformats',
            'kf6-kconfig-devel', 'kf6-kidletime-devel', 'kf6-kservice-devel',
            'kf6-kio-devel', 'kf6-kwindowsystem-devel', 'wayland-devel'],
    'arch': ['base-devel', 'cmake', 'ninja', 'pkgconf', 'qt6-base',
             'qt6-declarative', 'qt6-imageformats', 'kconfig', 'kidletime',
             'kservice', 'kio', 'kwindowsystem', 'wayland', 'breeze'],
}

def run(command, **kwargs):
    return subprocess.run(command, check=True, **kwargs)

def detect_family(path=Path('/etc/os-release')):
    fields = {}
    for line in path.read_text().splitlines():
        if '=' in line and not line.startswith('#'):
            name, value = line.split('=', 1)
            fields[name] = shlex.split(value)[0] if value else ''
    identifiers = [fields.get('ID', ''), *fields.get('ID_LIKE', '').split()]
    for ident in identifiers:
        if ident in {'debian', 'ubuntu'}:
            return 'deb'
        if ident in {'fedora', 'rhel', 'centos', 'rocky', 'almalinux'}:
            return 'rpm'
        if ident in {'arch', 'archlinux', 'cachyos', 'manjaro', 'endeavouros'}:
            return 'arch'
    raise ValueError('This distribution is not supported by the guided installer. See the manual build instructions in README.md.')

def package_commands(family, assume_yes=False):
    if family == 'deb':
        return [['sudo', 'apt-get', 'update'],
                ['sudo', 'apt-get', 'install', '-y', '--no-install-recommends', *PACKAGES[family]]]
    if family == 'rpm':
        return [['sudo', 'dnf', 'install', '-y', *PACKAGES[family]]]
    options = ['--noconfirm'] if assume_yes else []
    return [['sudo', 'pacman', '-Syu', '--needed', *options, *PACKAGES[family]]]

def check_desktop():
    shell = shutil.which('plasmashell')
    if not shell:
        raise ValueError('KDE Plasma 6 is required. Choose a distribution with a Plasma 6 desktop first; this installer does not install or replace your desktop.')
    result = run([shell, '--version'], capture_output=True, text=True, timeout=15,
                 env=dict(os.environ, QT_QPA_PLATFORM='minimal'))
    match = re.search(r'plasmashell\s+(\d+)\.(\d+)', result.stdout + result.stderr)
    if not match or int(match[1]) != 6:
        raise ValueError('KDE Plasma 6 is required. Plasma 5 and unrecognized Plasma versions are not supported.')
    if not shutil.which('plasma-apply-colorscheme'):
        raise ValueError('The Plasma color-scheme tool is missing. Repair your Plasma desktop installation before continuing.')
    return match[0]

def check_packages(family):
    """Query enabled repositories before attempting dependency installation."""
    packages = PACKAGES[family]
    if family == 'deb':
        result = run(['apt-cache', 'policy', *packages], capture_output=True, text=True,
                     env=dict(os.environ, LC_ALL='C'))
        available = set()
        current = None
        for line in result.stdout.splitlines():
            if line and not line[0].isspace() and line.endswith(':'):
                current = line[:-1].split(':')[0]
            elif 'Candidate:' in line and '(none)' not in line:
                available.add(current)
    elif family == 'rpm':
        result = subprocess.run(['dnf', '-q', 'list', *packages],
                                capture_output=True, text=True, check=False,
                                env=dict(os.environ, LC_ALL='C'))
        if result.returncode:
            raise ValueError('DNF could not find the required KDE 6 development packages in your enabled repositories. No repositories were added.\n' + result.stderr.strip())
        available = {line.split()[0].rsplit('.', 1)[0]
                     for line in result.stdout.splitlines() if line.split()}
    else:
        result = run(['pacman', '-Si', *packages], capture_output=True, text=True,
                     env=dict(os.environ, LC_ALL='C'))
        available = {line.split(':', 1)[1].strip()
                     for line in result.stdout.splitlines() if line.startswith('Name ')}
    missing = sorted(set(packages) - available)
    if missing:
        raise ValueError('Your enabled repositories lack required packages: ' + ', '.join(missing) +
                         '. KDE Frameworks 6 is required; KDE 5 packages cannot substitute. No repositories were added.')

def build_and_install(prefix, config_root, jobs):
    """Build for this machine, then reuse the transactional local installer."""
    with tempfile.TemporaryDirectory(prefix='kamakiri-install-') as directory:
        temporary = Path(directory)
        build = temporary / 'build'
        generator = 'Ninja' if shutil.which('ninja') else 'Unix Makefiles'
        run(['cmake', '-S', str(ROOT), '-B', str(build), '-G', generator,
             '-DCMAKE_BUILD_TYPE=Release', '-DBUILD_TESTING=OFF'])
        run(['cmake', '--build', str(build), '-j', str(jobs)])
        binary = build / 'kamakiri-studio'
        release = json.loads((ROOT / 'release.json').read_text())
        probe = run([str(binary), '--version'], capture_output=True, text=True,
                    timeout=15, env=dict(os.environ, QT_QPA_PLATFORM='offscreen'))
        if probe.stdout.strip() != 'KamaKiriStudio ' + release['version']:
            raise ValueError('The built application failed its version check. Your existing installation was not changed.')
        bundle = temporary / 'bundle'
        (bundle / 'bin').mkdir(parents=True)
        shutil.copy2(binary, bundle / 'bin/kamakiri-studio')
        shutil.copy2(ROOT / 'release.json', bundle / 'release.json')
        shutil.copytree(ROOT / 'assets', bundle / 'assets')
        run([sys.executable, str(ROOT / 'install-local.py'), '--bundle', str(bundle),
             '--prefix', str(prefix), '--config-root', str(config_root)])

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dry-run', action='store_true', help='Show the plan without installing or downloading anything.')
    parser.add_argument('--check', action='store_true', help='Check desktop and package availability without installing.')
    parser.add_argument('--yes', action='store_true', help='Accept dependency installation, including the full Arch system upgrade.')
    parser.add_argument('--skip-dependencies', action='store_true', help='Use dependencies already installed by an administrator.')
    parser.add_argument('--prefix', type=Path, default=Path.home() / '.local')
    parser.add_argument('--config-root', type=Path, default=Path(os.environ.get('XDG_CONFIG_HOME', str(Path.home() / '.config'))))
    parser.add_argument('--jobs', type=int, default=2)
    args = parser.parse_args()
    if os.getuid() == 0:
        raise ValueError('Run ./install.sh as your desktop user, without sudo. Only the package manager needs administrator access.')
    if args.jobs < 1:
        raise ValueError('--jobs must be at least 1.')
    family = detect_family()
    commands = package_commands(family, args.yes)
    print('Welcome to KamaKiriStudio.\nThis installs the app for your account and adds it to the Applications menu.\nYour desktop appearance stays unchanged.', flush=True)
    if json.loads((ROOT / 'release.json').read_text()).get('status') == 'development':
        print('This is an experimental development build. Use an isolated Plasma session for appearance trials.', flush=True)
    print('Installation folder:', args.prefix.expanduser().absolute())
    if not args.skip_dependencies:
        print('The following commands install build and runtime requirements:')
        for command in commands:
            print(' ', shlex.join(command))
        if family == 'arch':
            print('Arch also updates the system to avoid a partial upgrade.')
    print('The app is built for your system; this may take several minutes.')
    if args.dry_run:
        return
    print('Desktop:', check_desktop(), flush=True)
    if args.check:
        check_packages(family)
        print('Desktop and required packages are available. Nothing was installed.')
        return
    if not args.yes:
        if not sys.stdin.isatty():
            raise ValueError('Run ./install.sh in a terminal, or use --yes after reviewing --dry-run.')
        if input('Install KamaKiriStudio? [y/N] ').strip().lower() not in {'y', 'yes'}:
            print('Installation cancelled.')
            return
    if not args.skip_dependencies:
        manager = {'deb': 'apt-get', 'rpm': 'dnf', 'arch': 'pacman'}[family]
        if not shutil.which('sudo') or not shutil.which(manager):
            raise ValueError('Administrator access through sudo and ' + manager + ' is required. Ask your administrator to install the listed packages, then use --skip-dependencies.')
        if family == 'deb':
            run(commands[0])
        check_packages(family)
        run(commands[-1])
    build_and_install(args.prefix.expanduser().absolute(), args.config_root.expanduser().absolute(), args.jobs)
    print('\nReady. Open KamaKiriStudio from the Applications menu.\nStart with Preview; use Yes to keep a trial or No to restore your previous settings.')

if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print('Installation could not finish: ' + str(error), file=sys.stderr)
        sys.exit(1)
    except (KeyboardInterrupt, EOFError):
        print('\nInstallation cancelled.', file=sys.stderr)
        sys.exit(1)
