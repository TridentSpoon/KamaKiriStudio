#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
installer_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if [[ ! -t 0 && $# -eq 0 ]]; then
    if command -v konsole >/dev/null; then
        exec konsole --hold -e bash "$installer_dir/install.sh"
    fi
    echo 'Open a terminal in this folder and run: ./install.sh' >&2
    exit 1
fi
if (( EUID == 0 )); then
    echo 'Run ./install.sh as your desktop user, without sudo.' >&2
    exit 1
fi
if ! command -v python3 >/dev/null; then
    if command -v apt-get >/dev/null; then
        bootstrap=(sudo apt-get install -y python3)
    elif command -v dnf >/dev/null; then
        bootstrap=(sudo dnf install -y python3)
    elif command -v pacman >/dev/null; then
        bootstrap=(sudo pacman -Syu --needed python)
    else
        echo 'Python 3 is missing and no supported package manager was found.' >&2
        exit 1
    fi
    printf 'Python 3 is needed to run the installer. Install with: '
    printf '%q ' "${bootstrap[@]}"
    printf '\n'
    accepted=false
    for option in "$@"; do
        case "$option" in
            --dry-run) exit 0 ;;
            --check|--skip-dependencies|--help|-h)
                echo 'Install Python 3 first, then run this command again.' >&2
                exit 1 ;;
            --yes) accepted=true ;;
        esac
    done
    if ! command -v plasmashell >/dev/null; then
        echo 'KDE Plasma 6 is required. This installer does not install or replace your desktop.' >&2
        exit 1
    fi
    plasma_version="$(QT_QPA_PLATFORM=minimal plasmashell --version 2>&1)"
    if [[ ! "$plasma_version" =~ plasmashell[[:space:]]+6\. ]]; then
        echo 'KDE Plasma 6 is required; your installed Plasma version is unsupported.' >&2
        exit 1
    fi
    if ! command -v sudo >/dev/null; then
        echo 'Ask your administrator to install Python 3, then try again.' >&2
        exit 1
    fi
    if [[ "$accepted" == false ]]; then
        read -r -p 'Install Python 3 to continue? (Arch also updates the system.) [y/N] ' answer
        case "$answer" in [yY]|[yY][eE][sS]) ;; *) exit 0 ;; esac
    fi
    if [[ "${bootstrap[1]}" == apt-get ]]; then
        sudo apt-get update
    elif [[ "${bootstrap[1]}" == pacman && "$accepted" == true ]]; then
        bootstrap+=(--noconfirm)
    fi
    "${bootstrap[@]}"
fi
exec python3 "$installer_dir/install-easy.py" "$@"
