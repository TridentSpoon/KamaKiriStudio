#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Create a private KWin/Wayland session; never test against the user's session."""
import os
import shutil
from pathlib import Path
import subprocess
import sys
import tempfile

binary, client = map(lambda p: str(Path(p).resolve()), sys.argv[1:3])
with tempfile.TemporaryDirectory(prefix='kamakiri-frame-') as root:
    root = Path(root)
    runtime = root / 'runtime'
    runtime.mkdir(mode=0o700)
    script = root / 'session.sh'
    # Arguments are supplied through the environment, never interpolated shell commands.
    script.write_text('#!/bin/sh\nexport WAYLAND_DISPLAY=kamakiri-private-frame\n"$KAMAKIRI_TEST_CLIENT" "$KAMAKIRI_TEST_BINARY" > "$KAMAKIRI_TEST_RESULT" 2>&1\nresult=$?\nprintf "%s" "$result" > "$KAMAKIRI_TEST_EXIT"\nexit "$result"\n')
    script.chmod(0o700)
    original_binary = binary
    if len(sys.argv)>4:
        slot=root/'kamakiri-test-app'
        shutil.copy2(sys.argv[4],slot)
        binary=str(slot)
    env = dict(os.environ, XDG_RUNTIME_DIR=str(runtime), XDG_CONFIG_HOME=str(root/'config'),
               XDG_STATE_HOME=str(root/'state'), XDG_CACHE_HOME=str(root/'cache'), XDG_DATA_HOME=str(root/'data'),
               QT_NO_XDG_DESKTOP_PORTAL='1', QT_QPA_PLATFORM='wayland', QT_QPA_PLATFORMTHEME='', XDG_CURRENT_DESKTOP='', KWIN_COMPOSE='Q', KAMAKIRI_PRIVATE_FRAME_TEST='1',
               KAMAKIRI_TEST_CLIENT=client, KAMAKIRI_TEST_BINARY=binary,
               KAMAKIRI_TEST_RESULT=str(root/'result'), KAMAKIRI_TEST_EXIT=str(root/'exit'))
    if len(sys.argv)>4:
        env['KAMAKIRI_TEST_UPDATE_BINARY']=original_binary
    for key in ('DISPLAY','WAYLAND_DISPLAY','DBUS_SESSION_BUS_ADDRESS','QT_WAYLAND_SHELL_INTEGRATION'):
        env.pop(key, None)
    with (root/'kwin.log').open('w') as log:
        result = subprocess.run(['dbus-run-session','--','kwin_wayland','--virtual','--width','1280','--height','800',
            '--scale',sys.argv[3] if len(sys.argv)>3 else '1','--output-count','2','--socket','kamakiri-private-frame','--no-lockscreen','--no-global-shortcuts',
            '--no-kactivities','--exit-with-session',str(script)],env=env,stdout=log,stderr=log,timeout=55)
    print((root/'result').read_text() if (root/'result').exists() else (root/'kwin.log').read_text())
    code = int((root/'exit').read_text()) if (root/'exit').exists() else 99
    if code:
        print((root/'kwin.log').read_text()[-4000:])
        raise SystemExit(code)
