# Install KamaKiriStudio

KamaKiriStudio runs on your existing **KDE Plasma 6** desktop. The current 0.5.0 development version is experimental; test appearance trials in an isolated Plasma session.

## Before you start

Check **System Settings → About this System** for your Plasma version. Plasma 5 is unsupported. You need Internet access, free space for Qt/KDE development packages and a temporary build, and administrator access through `sudo` for dependencies. No additional repositories or desktop environments are installed.

| System | Package manager | Requirements |
| --- | --- | --- |
| Debian / Ubuntu and derivatives | APT | Plasma 6 and KDE Frameworks 6 development packages in enabled repositories |
| Fedora / RHEL and derivatives | DNF | Plasma 6 and KDE Frameworks 6 development packages in enabled repositories; many RHEL releases lack these |
| Arch / CachyOS / EndeavourOS / Manjaro | Pacman | Current Plasma 6; dependency installation includes a full system update to avoid partial upgrades |

These are installer paths, not a compatibility guarantee for every distribution release. The original cloud implementation reported installation on Debian 13. Fedora/RHEL dependency installation still needs validation on those systems.

## Install

1. Download and extract the source archive from the official [repository](https://github.com/TridentSpoon/KamaKiriStudio).
2. Open a terminal in the extracted folder. Dolphin's **F4** opens a terminal panel.
3. Run `./install.sh` without sudo. If necessary, use `bash install.sh`.
4. Review the plan and confirm. Only the package manager requests administrator access.
5. Wait for the build and installation. Open **KamaKiriStudio** from the Applications menu.

Installation leaves desktop appearance unchanged. The app and assets go under `~/.local`, with recovery registered at KDE login. The transactional installer refuses to overwrite files it does not own and restores its own writes if installation fails. Dependency packages remain installed if the subsequent build fails.

The extracted source folder can be deleted afterward; installed wallpapers and widgets remain. The optional local-source update check becomes unavailable when its source folder is removed.

## Check compatibility and administrator options

```sh
./install.sh --dry-run             # Show commands without changes or downloads
./install.sh --check               # Check Plasma and package availability
./install.sh --skip-dependencies   # Build with previously installed dependencies
```

`--yes` accepts installation, including Arch's full system upgrade. `--jobs N` controls build parallelism (default two). `--prefix PATH` and `--config-root PATH` select other installation locations. For a custom prefix, KDE may require its `share` directory in `XDG_DATA_DIRS` to discover assets and menu entries.

Requirements must come from your distribution's supported repositories. KDE 5 cannot substitute for KDE 6.

## Preview and recovery

Run `~/.local/bin/kamakiri-studio --demo` to preview without changing appearance. **Apply & try** starts a reversible trial: Yes keeps it and No restores previous settings. The initial 15-second timeout stops after your first input, so choose No explicitly to cancel afterward. Recovery can fail if Plasma or the original panel is unavailable; preserve recovery records when an error appears.
