#!/usr/bin/env python3
"""Isolated watchdog integration: no KDE setting changes or real session access."""
import json, os, pathlib, subprocess, tempfile, time, uuid, sys
binary = pathlib.Path(sys.argv[1]).resolve()

def atomic(path, value):
    stage = path.with_suffix('.new')
    stage.write_text(json.dumps(value))
    stage.chmod(0o600)
    stage.replace(path)

def run_case(name, drive, expected, duration):
    with tempfile.TemporaryDirectory(prefix='kamakiri-studio-test-') as temp:
        root=pathlib.Path(temp)
        env=dict(os.environ, QT_QPA_PLATFORM='offscreen', XDG_STATE_HOME=str(root/'state'), XDG_CONFIG_HOME=str(root/'config'), XDG_DATA_HOME=str(root/'data'), XDG_CACHE_HOME=str(root/'cache'))
        env.pop('DBUS_SESSION_BUS_ADDRESS',None)
        ident=str(uuid.uuid4())
        directory=root/'state/kamakiri-studio'/ident
        directory.mkdir(parents=True,mode=0o700)
        request=dict(preset='caelestia',accent='#c4a7ff',light=False,changePanel=False,demo=True)
        atomic(directory/'request.json',request)
        atomic(directory/'command.json',dict(heartbeat=1,input=False,decision=''))
        process=subprocess.Popen([str(binary),'--worker',ident,'--demo'],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        start=time.monotonic(); counter=1; interacted=False; pending_start=None; observed=[]
        while process.poll() is None and time.monotonic()-start < duration+4:
            elapsed=time.monotonic()-start
            status={}
            try:status=json.loads((directory/'status.json').read_text())
            except FileNotFoundError:pass
            if status.get('state')=='pending' and pending_start is None:pending_start=time.monotonic()
            observed.append(status.get('state'))
            trial=time.monotonic()-pending_start if pending_start else 0
            command=drive(trial,counter)
            if command is not None:
                counter+=1
                atomic(directory/'command.json',dict(heartbeat=counter,**command))
            if status.get('interacted'):interacted=True
            time.sleep(.05)
        if process.poll() is None:process.kill()
        out,err=process.communicate(timeout=3)
        status=json.loads((directory/'status.json').read_text())
        assert status['state']==expected,(name,status,out.decode(),err.decode())
        if name=='input-then-no':
            assert interacted and time.monotonic()-pending_start>16,(name,'input did not cancel deadline')
        if name=='no-input':assert time.monotonic()-pending_start>=15,(name,'reverted too early')
        assert not (root/'config/kdeglobals').exists(), 'Demo touched actual settings'
        print('PASS:',name,status['state'])

run_case('no-input',lambda t,c:dict(input=False,decision=''),'reverted',18)
run_case('input-then-no',lambda t,c:dict(input=t>0.5,decision='no' if t>16.2 else ''),'reverted',20)
run_case('explicit-yes',lambda t,c:dict(input=False,decision='yes' if t>.4 else ''),'kept',4)
run_case('owner-crash-after-input',lambda t,c:dict(input=t>.2,decision='') if t<.6 else None,'reverted',7)

# Simulate a watchdog crash, then exercise the login/startup recovery path.
with tempfile.TemporaryDirectory(prefix='kamakiri-recovery-test-') as temp:
    root=pathlib.Path(temp)
    env=dict(os.environ,QT_QPA_PLATFORM='offscreen',XDG_STATE_HOME=str(root/'state'),XDG_CONFIG_HOME=str(root/'config'),XDG_DATA_HOME=str(root/'data'),XDG_CACHE_HOME=str(root/'cache'))
    env.pop('DBUS_SESSION_BUS_ADDRESS',None)
    ident=str(uuid.uuid4());folder=root/'state/kamakiri-studio'/ident
    folder.mkdir(parents=True,mode=0o700)
    atomic(folder/'request.json',dict(preset='ryoku',accent='#eea3b2',changePanel=False,demo=True))
    atomic(folder/'command.json',dict(heartbeat=1,input=False,decision=''))
    process=subprocess.Popen([str(binary),'--worker',ident,'--demo'],env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    deadline=time.monotonic()+3
    while time.monotonic()<deadline:
        try:
            if json.loads((folder/'status.json').read_text())['state']=='pending':break
        except (FileNotFoundError,KeyError):pass
        time.sleep(.05)
    else:
        process.kill();process.wait();raise AssertionError('Watchdog did not reach pending')
    process.kill();process.wait()
    subprocess.run([str(binary),'--recover-all'],env=env,check=True)
    assert json.loads((folder/'status.json').read_text())['state']=='reverted'
    assert not (root/'config/kdeglobals').exists()
    print('PASS: abandoned-watchdog startup recovery reverted')
