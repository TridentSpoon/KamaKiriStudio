# SPDX-License-Identifier: MIT
# Optional native integration check: a private D-Bus bus and virtual KWin outputs.
import os,pathlib,subprocess,time,json,re,signal
import tempfile, shutil
repo=pathlib.Path(__file__).resolve().parents[1];os.chdir(repo)
binary=pathlib.Path(os.environ.get('KAMAKIRI_TEST_BINARY',str(repo/'bin/kamakiri-studio'))).resolve()
root=pathlib.Path(os.environ.get('KAMAKIRI_TEST_ROOT') or tempfile.mkdtemp(prefix='kamakiri-layout-'))
os.environ['KAMAKIRI_TEST_ROOT']=str(root)
for name in ('home','config','data','cache','state','runtime'):(root/name).mkdir(exist_ok=True,mode=0o700)
shutil.copytree(pathlib.Path('assets/desktoptheme'),root/'data/plasma/desktoptheme',dirs_exist_ok=True)
shutil.copytree(pathlib.Path('assets/plasmoids'),root/'data/plasma/plasmoids',dirs_exist_ok=True)
env=dict(os.environ,HOME=str(root/'home'),XDG_CONFIG_HOME=str(root/'config'),XDG_DATA_HOME=str(root/'data'),XDG_CACHE_HOME=str(root/'cache'),XDG_STATE_HOME=str(root/'state'),XDG_RUNTIME_DIR=str(root/'runtime'),QT_QPA_PLATFORM='offscreen',XDG_CURRENT_DESKTOP='KDE',XDG_SESSION_TYPE='wayland',WAYLAND_DISPLAY='ks',KWIN_COMPOSE='Q')
if not os.environ.get('KAMAKIRI_ISOLATED_BUS'):
 env['KAMAKIRI_ISOLATED_BUS']='1'
 raise SystemExit(subprocess.call(['dbus-run-session','--','python3',__file__],env=env))
log=(root/'services.log').open('w')
kwin=subprocess.Popen(['kwin_wayland','--virtual','--output-count','2','--width','800','--height','600','--socket','ks','--no-lockscreen','--no-global-shortcuts'],env=env,stdout=log,stderr=log)
plasma=None
try:
 for _ in range(100):
  if (root/'runtime/ks').exists():break
  if kwin.poll() is not None:raise RuntimeError('Isolated KWin failed; see services.log')
  time.sleep(.1)
 else:raise RuntimeError('No isolated Wayland socket')
 env['QT_QPA_PLATFORM']='wayland'
 plasma=subprocess.Popen(['plasmashell'],env=env,stdout=log,stderr=log)
 def script(js):return subprocess.check_output(['qdbus6','org.kde.plasmashell','/PlasmaShell','org.kde.PlasmaShell.evaluateScript',js],env=env,text=True,stderr=subprocess.DEVNULL,timeout=10).strip()
 for _ in range(100):
  try:script("print(screenCount)");break
  except Exception:time.sleep(.1)
 else:raise RuntimeError('No isolated Plasma scripting service')
 print('ISOLATED_PLASMA_READY',script('print(screenCount)'),flush=True)
 # The app and all writes use the isolated session; no live desktop is accessed.
 import uuid
 def run_trial(request,decision='no'):
  trial=root/'state/kamakiri-studio'/str(uuid.uuid4());trial.mkdir(parents=True,mode=0o700)
  def command(i,value):
   temp=trial/'command.tmp';temp.write_text(json.dumps({'heartbeat':i,'input':False,'decision':value}));temp.chmod(0o600);temp.replace(trial/'command.json')
  (trial/'request.json').write_text(json.dumps(request));command(1,'')
  process=subprocess.Popen([str(binary),'--worker',trial.name],env=env,stdout=log,stderr=log)
  for i in range(200):
   status=json.loads((trial/'status.json').read_text()) if (trial/'status.json').exists() else {}
   if status.get('state') in ['reverted','kept','failed','recovery-needed']:break
   if status.get('state')=='pending' and request.get('desktopLook') in ('fluent11','fluent10'):
    widgets=json.loads(script("print(JSON.stringify(panels().filter(function(p){p.currentConfigGroup=['General'];return p.id==="+json.dumps(request['panel']['id'])+";})[0].widgets().sort(function(a,b){return a.index-b.index;}).map(function(w){w.currentConfigGroup=['General'];return {type:w.type,url:w.readConfig('url','')};})))"))
    assert [w['type'] for w in widgets][-3:]==['org.kde.plasma.digitalclock','org.kde.plasma.notifications','org.kde.plasma.showdesktop'],widgets
    assert not any('kamakiri-launcher.desktop' in w['url'] or 'kamakiri-dashboard.desktop' in w['url'] for w in widgets),widgets
    time.sleep(1)
    def geometry():
     return json.loads(script("print(JSON.stringify(panels().filter(function(p){p.currentConfigGroup=['General'];return p.id==="+json.dumps(request['panel']['id'])+";})[0].widgets().map(function(w){return {type:w.type,geometry:w.geometry};})))"))
    def verify_positions():
     positions={w['type']:w['geometry'] for w in geometry()};peek=positions['org.kde.plasma.showdesktop'];clock=positions['org.kde.plasma.digitalclock'];notification=positions['org.kde.plasma.notifications'];tray=positions['org.kde.plasma.systemtray']
     assert peek['x']+peek['width']>=780,positions
     assert tray['x']<clock['x']<notification['x']<peek['x'],positions
     if request['desktopLook']=='fluent10':assert positions['org.kamakiri.start']['x']<=16,positions
     print('GEOMETRY_VERIFIED',request['desktopLook'],positions,flush=True)
    verify_positions()
    if request['desktopLook']=='fluent10':
     apps=[]
     try:
      for n in range(2):
       qml=root/f'window-{n}.qml';qml.write_text('import QtQuick\nimport QtQuick.Window\nWindow { visible: true; width: 200; height: 120; title: "Virtual layout test" }')
       apps.append(subprocess.Popen(['/usr/lib/qt6/bin/qml',str(qml)],env=env,stdout=log,stderr=log))
      time.sleep(1);assert all(app.poll() is None for app in apps);verify_positions();print('OPEN_WINDOWS_PLACEMENT_VERIFIED',flush=True)
     finally:
      for app in apps:
       app.terminate();app.wait(timeout=5)
    print('FLUENT_RIGHT_EDGE_VERIFIED',request['desktopLook'],flush=True)
   command(i+2,decision if status.get('state')=='pending' else '');time.sleep(.1)
  process.wait(timeout=10);return status,trial.name
 created={'preset':'breeze','accent':'#6699cc','changeColors':False,'changePanel':True,'panels':[],'panel':{'id':0,'location':'bottom','height':48,'floating':False},'removePanelIds':[],'createPanels':[{'screen':1,'location':'bottom','height':48,'floating':False}],'changePopupLook':False,'changeWallpaper':False,'widgets':[],'popupMode':'desktop'}
 # Seed a populated existing taskbar to cover the user's failure case.
 existing=int(script("var p=new Panel();p.screen=1;p.location='bottom';p.height=48;['org.kde.plasma.kickoff','org.kde.plasma.icontasks','org.kde.plasma.systemtray','org.kde.plasma.digitalclock','org.kde.plasma.showdesktop','org.kde.plasma.notifications','org.kde.plasma.icon','org.kde.plasma.panelspacer'].forEach(function(t){p.addWidget(t);});print(p.id);"))
 time.sleep(2)
 created['createPanels']=[];created['panel']['id']=existing;created['panels']=[created['panel']]
 for look in ('fluent10','fluent11','fluent10'):
  candidate=dict(created,desktopLook=look);status,_=run_trial(candidate);assert status['state']=='reverted',status
  print('EXISTING_PANEL_ROLLBACK_VERIFIED',look,flush=True)


finally:
 for p in (plasma,kwin):
  if p and p.poll() is None:
   p.terminate()
   try:p.wait(timeout=5)
   except subprocess.TimeoutExpired:p.kill();p.wait()
 log.close()
