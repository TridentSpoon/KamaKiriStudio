# Validation — KamaKiriStudio 0.1.0

Date: 3 October 2026.

Test platform: CachyOS, Plasma workspace 6.7.5, Qt 6.11.2, KDE IdleTime 6.30.0, libwayland 1.26.0, x86-64. Plasma was running on Wayland. The installed session list contained only Plasma (Wayland).

## Passed

- Native C/C++ release build with compiler warnings enabled.
- Nine unit checks: the exact 15-second deadline, indefinite waiting after input, restoration on No/owner loss, keeping on Yes, safe JavaScript data quoting, invalid panel/preset/accent rejection, restoration of nested color groups without overwriting unrelated preferences, reading session definitions without executing them, and private atomic recovery records.
- Five isolated watchdog-process integration scenarios: no-input restoration at 15 seconds; input followed by waiting beyond 16 seconds then No; explicit Yes; owner heartbeat loss after interaction; and startup recovery after the watchdog itself is killed.
- Native GUI preview and nonmodal Yes/No flow, captured offscreen and visually inspected.
- Read-only live Plasma integration: both existing panels discovered, KDE color application tool found, and installed session discovered.
- Input-only Wayland notifier version 2 successfully bound in KWin. Its input-only request ignores idle inhibitors; monitoring receives activity notifications without key contents or pointer positions.
- A live appearance trial using the same backend: original palette snapshotted, trial palette applied, one existing panel's floating flag toggled, then explicit No restored the original palette and panel state. Every parsed key/value in kdeglobals matched the pre-trial snapshot afterward. No lasting appearance changes remained.
- Per-user installer exercised in a disposable prefix, including repeat installation and desktop-file validation.

## Limits

This is an initial application, not a security certification. The isolated process tests simulate the input/heartbeat data; physical cross-window input has not been automated. The compositor input capability was verified separately, and the real apply-and-restore path was exercised. X11 behavior, other distributions/Plasma versions, logging into an alternative desktop/WM, power loss, and destruction of a panel during a pending trial were not live-tested.

Desktop session switching deliberately uses KDE's logout prompt and the normal login-screen session chooser. It does not automatically select the target session, replace the display manager, install other desktops, or swap KWin in place.

Rollback restores the palette keys and selected panel properties this app changes. Restoration can fail if the original panel disappears, Plasma stops responding, configuration becomes unwritable, or recovery snapshots are removed. Such failures preserve recovery data and are reported rather than marked successful. A second trial is blocked while a previous one is unfinished. Recovery at next login requires the local installer’s autostart entry; opening the portable manager also checks abandoned trials.

The included executable uses the installed system Qt/KDE/Wayland libraries and should be rebuilt on other systems. No reviewed Caelestia KDE executable code was incorporated.

## Version 0.2.0 palette update

All 22 upstream palettes pass color validation and native KDE file generation checks for background, foreground and selection. A regression test now verifies serialized RGB values, correcting the previous QColor saving issue. Existing confirmation-policy, rollback and hostile-input tests pass. The isolated GUI confirmation check passes and the Osaka Jade preview has been visually checked. The update does not apply a live appearance trial automatically.

## Version 0.3.0 desktop features

Unit checks validate local image formats and dimensions, deterministic wallpaper palettes, script quoting, widget allowlists, duplicate rejection and ownership markers. The live KDE test applied a wallpaper-derived palette and a new desktop widget, verified the owned widget, chose No and compared the desktop/widget inventory and all parsed kdeglobals values with the baseline: exact match. The native settings modules used by the dashboard are installed on the test machine. GUI previews of the dashboard and wallpaper/widget page were rendered and inspected. MPRIS button integration still requires a running player; no player action was simulated against a real user playback session. The app provides KDE equivalents, not Caelestia animations.
