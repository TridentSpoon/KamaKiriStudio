# Development status

The 0.5.0 source snapshot is merged for ongoing private development; it is not a validated stable release.

- Theme switching can crash or hang; the exact cause is not established.
- Appearance workers using different state roots can overlap. A shared session lock is needed for live application and recovery.
- Interrupted native launcher restoration reported an unavailable original taskbar widget. Keep recovery records; do not reset the desktop or discard snapshots.
- Repeated look application can accumulate owned spacers or legacy controls. Idempotent application and rollback coverage need further work.
- Closed standalone launcher/dashboard tool windows can leave background processes running. Explicit shutdown and single-instance handling need verification.
- A temporary test panel can remain after interruption. Remove only positively identified test panels after completing recovery.

Panel selection now prefers an existing panel at the requested edge. Whole-look selection no longer enables duplicate standalone launcher/dashboard buttons automatically. These changes are built and unit-tested but have not been live-applied.

Default Plasma uses native KDE Application Launcher and Breeze styling; Kamakiri style replaces the previous Caelestia label. Legacy identifiers remain compatible with existing profiles. Windows-style menus are approximations using KDE components; exact Windows parity has not been achieved.

Validation: local build and existing core/popup tests pass. The native Start preview loaded its application catalog with preview actions disabled. One temporary-panel Fluent 11 trial restored launcher configuration, native settings and colors. The subsequent interrupted trial did not complete recovery, so complete desktop-look recovery is not validated.

Use an isolated Plasma session or VM for further live tests. Do not publish a stable 0.5.0 release until the issues above are resolved.
