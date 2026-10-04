// SPDX-License-Identifier: MIT
#include "core.h"
#include <KConfig>
#include <KConfigGroup>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QFileInfo>
#include <QProcess>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QRegularExpression>
#include <sys/prctl.h>
#include <unistd.h>
#include <signal.h>
#include <stdexcept>
namespace Studio {
static void bad(const QString &s){throw std::runtime_error(s.toStdString());}
QJsonObject desktopPalette(const QString &preset,const QString &mode) {
    auto p=omarchyPalette(preset);
    if(preset=="windows")p={{"background","#202020"},{"dark_background","#191919"},{"lighter_background","#303030"},{"foreground","#f5f5f5"},{"dark_foreground","#b4b4b4"},{"accent","#0078d4"},{"selection","#174d76"},{"magenta","#b47ae6"},{"red","#e74856"},{"orange","#f7ad45"},{"green","#60b66e"},{"mode","dark"}};
    if(!p.isEmpty()&&mode!="desktop") {
        bool light=mode=="light";p["mode"]=mode;p["background"]=light?"#f3f3f3":"#202020";p["dark_background"]=light?"#ffffff":"#191919";
        p["lighter_background"]=light?"#e9e9e9":"#303030";p["foreground"]=light?"#191919":"#f5f5f5";p["dark_foreground"]=light?"#606060":"#b4b4b4";
        QColor accent(p["accent"].toString());p["selection"]=accent.name();
    }
    return p;
}
QStringList desktopLooks(){return {"plasma","caelestia","fluent11","fluent10"};}
QString associatedWallpaper(const QString &preset) {
    auto catalog=QStandardPaths::locate(QStandardPaths::GenericDataLocation,"kamakiri-studio/wallpapers/catalog.json");
    if(catalog.isEmpty())return {};
    auto name=readJson(catalog)[preset].toString();
    if(name.isEmpty()||name.contains('/')||name.contains(".."))return {};
    auto path=QFileInfo(catalog).absolutePath()+"/"+name;
    return QFileInfo(path).isFile()?path:QString();
}
static QString captureScript(int id){
    if(id<0)bad("Invalid look panel.");
    return QString(R"JS(try {var p=panelById(%1);if(!p)throw new Error('Panel unavailable');
function config(w,path,depth){if(depth>12)throw new Error('Configuration too deep');w.currentConfigGroup=path;var keys=w.configKeys;var groups=w.configGroups;var entries={};keys.forEach(function(k){if(k!=='KamaKiriRestoredFrom')entries[k]=w.readConfig(k,'');});return {path:path,entries:entries,children:groups.map(function(g){return config(w,path.concat([g]),depth+1);})};}
var launchers=['org.kde.plasma.kickoff','org.kde.plasma.kicker','org.kde.plasma.kickerdash','org.kamakiri.start'];
print(JSON.stringify({id:p.id,alignment:p.alignment,lengthMode:p.lengthMode,length:p.length,offset:p.offset,hiding:p.hiding,widgets:p.widgets().sort(function(a,b){return a.index-b.index;}).map(function(w){return {id:w.id,type:w.type,index:w.index,shortcut:w.globalShortcut,launcher:launchers.indexOf(w.type)>=0,config:launchers.indexOf(w.type)>=0?config(w,[],0):null};})}));}catch(e){print('STUDIO_ERROR:'+e);})JS").arg(id);
}
static const QList<QStringList> settingsKeys{
    {"plasmarc","Theme","name"}, {"kdeglobals","KDE","widgetStyle"}, {"kdeglobals","Icons","Theme"},
    {"kwinrc","org.kde.kdecoration2","library"},{"kwinrc","org.kde.kdecoration2","theme"},{"kwinrc","org.kde.kdecoration2","BorderSize"},
    {"kwinrc","org.kde.kdecoration2","ButtonsOnLeft"},{"kwinrc","org.kde.kdecoration2","ButtonsOnRight"}
};
QJsonObject snapshotLook(int panelId) {
    auto doc=QJsonDocument::fromJson(plasmaScript(captureScript(panelId)).trimmed().toUtf8());
    if(!doc.isObject())bad("Cannot save the original launcher and taskbar.");
    QJsonArray settings;
    for(const auto &key:settingsKeys){KConfig c(key[0],KConfig::SimpleConfig);auto g=c.group(key[1]);settings.append(QJsonObject{{"file",key[0]},{"group",key[1]},{"key",key[2]},{"present",g.hasKey(key[2])},{"value",g.readEntry(key[2],QString())}});}
    return {{"panel",doc.object()},{"settings",settings}};
}
static void applyTheme(const QString &name) {
    if(!QRegularExpression("^[a-zA-Z0-9_.-]+$").match(name).hasMatch())bad("Invalid Plasma style name.");
    auto tool=QStandardPaths::findExecutable("plasma-apply-desktoptheme");if(tool.isEmpty())bad("KDE's Plasma style tool is missing.");
    QProcess process;pid_t parent=getpid();process.setChildProcessModifier([parent]{if(prctl(PR_SET_PDEATHSIG,SIGKILL)!=0||getppid()!=parent)_exit(127);});process.start(tool,{name});
    if(!process.waitForFinished(8000)){process.kill();process.waitForFinished();bad("Plasma style application timed out.");}
    if(process.exitStatus()!=QProcess::NormalExit||process.exitCode()!=0)bad("Plasma refused the desktop style.");
}
static void notifyLook(){notifyPalette();for(int type:{2,4}){auto change=QDBusMessage::createSignal("/KGlobalSettings","org.kde.KGlobalSettings","notifyChange");change<<type<<0;QDBusConnection::sessionBus().send(change);}QDBusConnection::sessionBus().send(QDBusMessage::createMethodCall("org.kde.KWin","/KWin","org.kde.KWin","reconfigure"));}
QString applyLookScript(int id,const QString &look,const QString &mode,const QString &owner) {
    if(id<0||!desktopLooks().contains(look)||!QStringList{"desktop","light","dark"}.contains(mode)||!QRegularExpression("^[a-f0-9-]{36}$").match(owner).hasMatch())bad("Invalid desktop look.");
    return QString(R"JS(try {var p=panelById(%1);if(!p)throw new Error('Panel unavailable');var look=%2;var mode=%3;var owner=%4;
var required=[look==='plasma'?'org.kde.plasma.kickoff':'org.kamakiri.start','org.kde.plasma.panelspacer','org.kde.plasma.icontasks','org.kde.plasma.systemtray','org.kde.plasma.digitalclock'];required.forEach(function(t){if(knownWidgetTypes.indexOf(t)<0)throw new Error('Required widget unavailable: '+t);});
function add(type){var w=p.addWidget(type);w.currentConfigGroup=['General'];w.writeConfig('KamaKiriTransaction',owner);if(w.readConfig('KamaKiriTransaction','')!==owner){w.remove();throw new Error('Cannot mark look widget');}return w;}
var old=p.widgets();var start=add(look==='plasma'?'org.kde.plasma.kickoff':'org.kamakiri.start');if(look!=='plasma'){start.writeConfig('look',look);start.writeConfig('mode',mode);}
var launcherTypes=['org.kde.plasma.kickoff','org.kde.plasma.kicker','org.kde.plasma.kickerdash','org.kamakiri.start'];var favorites=[];
old.forEach(function(w){if(launcherTypes.indexOf(w.type)>=0){w.currentConfigGroup=['General'];var f=w.readConfig('favorites',[]);if(f&&f.length)favorites=f;}});
if(favorites.length)start.writeConfig('favorites',favorites);
var tasks=p.widgets('org.kde.plasma.icontasks')[0]||p.widgets('org.kde.plasma.taskmanager')[0]||add('org.kde.plasma.icontasks');
var tray=p.widgets('org.kde.plasma.systemtray')[0]||add('org.kde.plasma.systemtray');var clock=p.widgets('org.kde.plasma.digitalclock')[0]||add('org.kde.plasma.digitalclock');
var order=[];if(look==='fluent11')order.push(add('org.kde.plasma.panelspacer'));
order.push(start);order.push(tasks);order.push(add('org.kde.plasma.panelspacer'));order.push(tray);order.push(clock);
old.forEach(function(w){if(launcherTypes.indexOf(w.type)>=0)w.remove();else if(order.indexOf(w)<0)order.push(w);});
order.forEach(function(w,i){w.index=i;});start.globalShortcut='Alt+F1';
p.alignment='center';p.offset=0;p.lengthMode='fill';p.hiding='none';print('STUDIO_OK');}catch(e){print('STUDIO_ERROR:'+e);})JS").arg(id).arg(jsonString(look),jsonString(mode),jsonString(owner));
}
void applyLook(const QJsonObject &r,const QString &owner) {
    QString look=r["desktopLook"].toString();if(!desktopLooks().contains(look))bad("Unknown desktop look.");
    if(look!="plasma"&&QStandardPaths::locate(QStandardPaths::GenericDataLocation,"plasma/desktoptheme/kamakiri-"+look+"/metadata.json").isEmpty())bad("Install the desktop-look assets first.");
    applyTheme(look=="plasma"?"default":"kamakiri-"+look);
    for(const auto &entry:QList<QStringList>{{"kdeglobals","KDE","widgetStyle","Breeze"},{"kdeglobals","Icons","Theme","breeze"},{"kwinrc","org.kde.kdecoration2","library","org.kde.breeze"},{"kwinrc","org.kde.kdecoration2","theme",""},{"kwinrc","org.kde.kdecoration2","BorderSize","None"},{"kwinrc","org.kde.kdecoration2","ButtonsOnLeft",""},{"kwinrc","org.kde.kdecoration2","ButtonsOnRight","IAX"}}){KConfig c(entry[0],KConfig::SimpleConfig);c.group(entry[1]).writeEntry(entry[2],entry[3],KConfig::Notify);if(!c.sync())bad("Cannot apply native desktop styling.");}
    notifyLook();plasmaScript(applyLookScript(r["panel"].toObject()["id"].toInt(),look,r["popupMode"].toString(),owner));
    bool found=false;for(const auto &p:desktopInventory()["panelWidgets"].toArray())if(p.toObject()["id"]==r["panel"].toObject()["id"])for(const auto &w:p.toObject()["widgets"].toArray())if(w.toObject()["type"]==(look=="plasma"?"org.kde.plasma.kickoff":"org.kamakiri.start")&&w.toObject()["owner"]==owner)found=true;
    if(!found)bad("The new Start menu could not be verified.");
}
QString restoreLookScript(const QJsonObject &saved,const QString &owner) {
    auto p=saved["panel"].toObject();if(p["id"].toInt(-1)<0||!QRegularExpression("^[a-f0-9-]{36}$").match(owner).hasMatch())bad("Invalid desktop look recovery record.");
    return QString(R"JS(try {var old=%1;var owner=%2;var p=panelById(old.id);if(!p)throw new Error('Original panel unavailable');
p.widgets().forEach(function(w){w.currentConfigGroup=['General'];if(w.readConfig('KamaKiriTransaction','')===owner)w.remove();});
function restore(w,node){w.currentConfigGroup=node.path;Object.keys(node.entries).forEach(function(k){w.writeConfig(k,node.entries[k]);});node.children.forEach(function(c){restore(w,c);});}
var order=[];old.widgets.forEach(function(s){var w=p.widgetById(s.id);if(!w&&s.launcher){w=p.widgets().filter(function(a){a.currentConfigGroup=['General'];return a.readConfig('KamaKiriRestoredFrom','')===owner+':'+s.id;})[0];if(!w){w=p.addWidget(s.type);w.currentConfigGroup=['General'];w.writeConfig('KamaKiriRestoredFrom',owner+':'+s.id);}restore(w,s.config);w.globalShortcut=s.shortcut;}if(!w)throw new Error('Original taskbar widget unavailable');order.push({w:w,index:s.index});});order.sort(function(a,b){return a.index-b.index;});order.forEach(function(v){v.w.index=v.index;});
p.alignment=old.alignment;p.offset=old.offset;p.lengthMode=old.lengthMode;if(old.lengthMode==='custom')p.length=old.length;p.hiding=old.hiding;print('STUDIO_OK');}catch(e){print('STUDIO_ERROR:'+e);})JS").arg(QString::fromUtf8(QJsonDocument(p).toJson(QJsonDocument::Compact)),jsonString(owner));
}
void restoreLook(const QJsonObject &saved,const QString &owner) {
    if(saved.isEmpty())bad("Desktop look snapshot is missing.");
    QString theme="default";
    for(const auto &v:saved["settings"].toArray()){auto e=v.toObject();if(e["file"]=="plasmarc"&&e["key"]=="name"&&!e["value"].toString().isEmpty())theme=e["value"].toString();}
    applyTheme(theme);
    for(const auto &v:saved["settings"].toArray()){auto e=v.toObject();QStringList identity{e["file"].toString(),e["group"].toString(),e["key"].toString()};if(!settingsKeys.contains(identity))bad("Unexpected desktop styling recovery key.");KConfig c(identity[0],KConfig::SimpleConfig);auto g=c.group(identity[1]);if(e["present"].toBool())g.writeEntry(identity[2],e["value"].toString(),KConfig::Notify);else g.deleteEntry(identity[2],KConfig::Notify);if(!c.sync())bad("Cannot restore desktop styling.");}
    notifyLook();plasmaScript(restoreLookScript(saved,owner));
    auto restored=snapshotLook(saved["panel"].toObject()["id"].toInt());
    if(restored["settings"]!=saved["settings"])bad("Desktop styling restoration could not be verified.");
    auto oldPanel=saved["panel"].toObject(),newPanel=restored["panel"].toObject();
    auto oldWidgets=oldPanel["widgets"].toArray(),newWidgets=newPanel["widgets"].toArray();
    if(oldWidgets.size()!=newWidgets.size())bad("Original taskbar widget count differs after restoration.");
    for(int i=0;i<oldWidgets.size();i++){auto a=oldWidgets[i].toObject(),b=newWidgets[i].toObject();if(a["type"]!=b["type"]||a["config"]!=b["config"]||a["shortcut"]!=b["shortcut"]||a["index"]!=b["index"]||(!a["launcher"].toBool()&&a["id"]!=b["id"]))bad("Original taskbar settings could not be verified.");}
    for(const auto &key:{"alignment","lengthMode","offset","hiding"})if(oldPanel[key]!=newPanel[key])bad("Original taskbar placement could not be verified.");
}
}
