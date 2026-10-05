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

## Version 0.4.0 look and layout

Both unit suites pass, including Fluent Light/Dark foreground/background readability and transparent popup corners. The five simulated watchdog cases pass. A live reference-rail trial verified both owned panel buttons, kept all parsed kdeglobals values unchanged during the trial, chose No, and restored the complete panel geometry and widget inventory exactly. Light/Dark popup renders were inspected and calendar background/arrow contrast corrected. The compositor blur request is supported by KDE’s WindowSystem API; visible blur and popup placement depend on KWin and were not measured by the offscreen captures.

0.4.1: build and both CTest suites pass. Isolated GUI trial check confirms the manager has no stylesheet override and inherits the application palette after changing popup styles; No restores the trial.

## Monitor controls and panel removal

A separate virtual KWin/Plasma session with two outputs, a private D-Bus bus and private XDG configuration verified the native worker flow for secondary panel removal and No restoration, primary-panel removal refusal, and new panel creation followed by No cleanup. Restored widgets retain their positions and configuration; system-tray child applets are matched by plugin because their internal IDs change. Live user panels were not used for these checks. Automated tests also cover excluded wallpaper displays, legacy panel snapshots and configuration comparison.

Appearance consolidation: standalone wallpaper/widget navigation removed; local images and widget controls now live under Appearance Advanced settings. Isolated offscreen preview confirms the three remaining pages. The catalog test loads every selectable image and checks five per named theme, eight for Retro 82 and seven for Tokyo Night, and rejects the two removed branded Rose Pine/Lumon selections.

Fluent panel placement: an isolated two-display Plasma session verifies Fluent 11 and Fluent 10 end with native digital clock/calendar followed by Desktop Peek, contain no KamaKiri popup shortcut buttons, and restore their pre-trial configuration on No. The manager restricts popup shortcut buttons to Kamakiri style; obsolete layout spacers are rebuilt rather than accumulated.

Fluent placement regression: expanding spacers are explicitly enabled. On an 800px virtual display, Fluent 10 Start is at x=8 and Desktop Peek ends at x=792. Opening two isolated Qt windows expands tasks from 272px to 325px while the tray, clock, notification and Peek positions stay fixed. Both Fluent layouts pass No rollback. Reproduce with `KAMAKIRI_TEST_BINARY=/absolute/path/to/kamakiri-studio python3 tests/virtual_panel_check.py`; requires KWin, Plasma, qdbus6 and Qt qml. It uses a fresh temporary HOME and private D-Bus session.

Populated-panel regression: widget identities are compared by numeric Plasma ID, not JavaScript wrapper object equality. An existing panel with Start, tasks, tray, clock, Peek, notifications, an unrelated icon and an old spacer passes Fluent 10 → Fluent 11 → Fluent 10 position checks and No restoration. Tray recovery captures only the hidden-items setting changed by the layout and accepts older snapshots that did not capture it.

Recovery metadata regression: ignore only ConfigDialog/DialogHeight and DialogWidth, which Plasma adds when its widget settings window opens. A unit test reproduces the extra group and verifies that changed Start-menu look/favorites still fail comparison. The saved snapshot is preserved.

Fluent 11 Start-popup centering uses an invisible anchor at the Windows logo midpoint and assigns it to the native AppletPopup visualParent on expansion. Fluent 10 retains its normal Start-button anchor. Unit suites and virtual panel rollback passed; exact popup coordinates were not available from the virtual-session instrumentation and need a manual visual check.
