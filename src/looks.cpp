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
#include <algorithm>
#include <QThread>
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
QStringList themeWallpapers(const QString &preset) {
    QString catalog=QStringLiteral(STUDIO_ASSET_DIRECTORY)+"/wallpapers/catalog.json";
    if(!QFileInfo::exists(catalog))catalog=QStandardPaths::locate(QStandardPaths::GenericDataLocation,"kamakiri-studio/wallpapers/catalog.json");
    if(catalog.isEmpty())return {};
    QJsonValue value=readJson(catalog).value(preset);QJsonArray names=value.isArray()?value.toArray():QJsonArray{value};QStringList result;
    for(const auto &nameValue:names){auto name=nameValue.toString();if(name.isEmpty()||name.contains('/')||name.contains(".."))continue;auto path=QFileInfo(catalog).absolutePath()+"/"+name;if(QFileInfo(path).isFile())result.append(path);}
    return result;
}
QString associatedWallpaper(const QString &preset){auto paths=themeWallpapers(preset);return paths.isEmpty()?QString():paths.first();}
static QString captureScript(int id,bool full=false){
    if(id<0)bad("Invalid look panel.");
    return QString(R"JS(try {var p=panelById(%1);if(!p)throw new Error('Panel unavailable');
function config(w,path,depth){if(depth>12)throw new Error('Configuration too deep');w.currentConfigGroup=path;var keys=Array.prototype.slice.call(w.configKeys);var groups=Array.prototype.slice.call(w.configGroups);var entries={};keys.forEach(function(k){if(k!=='KamaKiriRestoredFrom')entries[k]=w.readConfig(k,'');});return {path:path,entries:entries,children:groups.map(function(g){return config(w,path.concat([g]),depth+1);})};}
var launchers=['org.kde.plasma.kickoff','org.kde.plasma.kicker','org.kde.plasma.kickerdash','org.kamakiri.start'];
function mutable(w){if(launchers.indexOf(w.type)>=0||w.type==='org.kde.plasma.panelspacer')return true;if(w.type!=='org.kde.plasma.icon')return false;w.currentConfigGroup=['General'];return /\/kamakiri-(launcher|dashboard)\.desktop$/.test(w.readConfig('url',''));}
function trayConfig(w){w.currentConfigGroup=['General'];return {path:['General'],entries:{hiddenItems:w.readConfig('hiddenItems',[])},children:[]};}
print(JSON.stringify({id:p.id,full:%2,screen:p.screen,location:p.location,height:p.height,floating:p.floating,alignment:p.alignment,lengthMode:p.lengthMode,length:p.length,offset:p.offset,hiding:p.hiding,widgets:p.widgets().sort(function(a,b){return a.index-b.index;}).map(function(w){return {id:w.id,type:w.type,index:w.index,shortcut:w.globalShortcut,launcher:mutable(w),config:%2||mutable(w)?config(w,[],0):w.type==='org.kde.plasma.systemtray'?trayConfig(w):null};})}));}catch(e){print('STUDIO_ERROR:'+e);})JS").arg(id).arg(full?"true":"false");
}
static const QList<QStringList> settingsKeys{
    {"plasmarc","Theme","name"}, {"kdeglobals","KDE","widgetStyle"}, {"kdeglobals","Icons","Theme"},
    {"kwinrc","org.kde.kdecoration2","library"},{"kwinrc","org.kde.kdecoration2","theme"},{"kwinrc","org.kde.kdecoration2","BorderSize"},
    {"kwinrc","org.kde.kdecoration2","ButtonsOnLeft"},{"kwinrc","org.kde.kdecoration2","ButtonsOnRight"}
};
QJsonObject snapshotDesktopStyling() {
    QJsonArray settings;
    for(const auto &key:settingsKeys){KConfig c(key[0],KConfig::SimpleConfig);auto g=c.group(key[1]);settings.append(QJsonObject{{"file",key[0]},{"group",key[1]},{"key",key[2]},{"present",g.hasKey(key[2])},{"value",g.readEntry(key[2],QString())}});}
    return {{"settings",settings}};
}
QJsonObject snapshotLook(int panelId,bool full) {
    auto doc=QJsonDocument::fromJson(plasmaScript(captureScript(panelId,full)).trimmed().toUtf8());
    if(!doc.isObject())bad("Cannot save the original launcher and taskbar.");
    auto saved=snapshotDesktopStyling();saved["panel"]=doc.object();return saved;
}
static void applyTheme(const QString &name) {
    if(!QRegularExpression("^[a-zA-Z0-9_.-]+$").match(name).hasMatch())bad("Invalid Plasma style name.");
    if(KConfig("plasmarc",KConfig::SimpleConfig).group("Theme").readEntry("name",QString("default"))==name)return;
    auto tool=QStandardPaths::findExecutable("plasma-apply-desktoptheme");if(tool.isEmpty())bad("KDE's Plasma style tool is missing.");
    QProcess process;pid_t parent=getpid();process.setChildProcessModifier([parent]{if(prctl(PR_SET_PDEATHSIG,SIGKILL)!=0||getppid()!=parent)_exit(127);});process.start(tool,{name});
    if(!process.waitForFinished(8000)){process.kill();process.waitForFinished();bad("Plasma style application timed out.");}
    if(process.exitStatus()!=QProcess::NormalExit||process.exitCode()!=0)bad("Plasma refused the desktop style.");
}
static void notifyLook(){notifyPalette();for(int type:{2,4}){auto change=QDBusMessage::createSignal("/KGlobalSettings","org.kde.KGlobalSettings","notifyChange");change<<type<<0;QDBusConnection::sessionBus().send(change);}QDBusConnection::sessionBus().send(QDBusMessage::createMethodCall("org.kde.KWin","/KWin","org.kde.KWin","reconfigure"));}
QString applyLookScript(int id,const QString &look,const QString &mode,const QString &owner) {
    if(id<0||!desktopLooks().contains(look)||!QStringList{"desktop","light","dark"}.contains(mode)||!QRegularExpression("^[a-f0-9-]{36}$").match(owner).hasMatch())bad("Invalid desktop look.");
    return QString(R"JS(try {var p=panelById(%1);if(!p)throw new Error('Panel unavailable');var look=%2;var mode=%3;var owner=%4;
var fluent=look==='fluent11'||look==='fluent10';var required=[look==='plasma'?'org.kde.plasma.kickoff':'org.kamakiri.start','org.kde.plasma.panelspacer','org.kde.plasma.icontasks','org.kde.plasma.systemtray','org.kde.plasma.digitalclock'];if(fluent){required.push('org.kde.plasma.showdesktop');required.push('org.kde.plasma.notifications');}required.forEach(function(t){if(knownWidgetTypes.indexOf(t)<0)throw new Error('Required widget unavailable: '+t);});
function add(type){var w=p.addWidget(type);w.currentConfigGroup=['General'];w.writeConfig('KamaKiriTransaction',owner);if(w.readConfig('KamaKiriTransaction','')!==owner){w.remove();throw new Error('Cannot mark look widget');}return w;}
var old=p.widgets();var start=add(look==='plasma'?'org.kde.plasma.kickoff':'org.kamakiri.start');if(look!=='plasma'){start.writeConfig('look',look);start.writeConfig('mode',mode);}
var launcherTypes=['org.kde.plasma.kickoff','org.kde.plasma.kicker','org.kde.plasma.kickerdash','org.kamakiri.start'];var favorites=[];
old.forEach(function(w){if(launcherTypes.indexOf(w.type)>=0){w.currentConfigGroup=['General'];var f=w.readConfig('favorites',[]);if(f&&f.length)favorites=f;}});
if(favorites.length)start.writeConfig('favorites',favorites);
var tasks=p.widgets('org.kde.plasma.icontasks')[0]||p.widgets('org.kde.plasma.taskmanager')[0]||add('org.kde.plasma.icontasks');
var tray=p.widgets('org.kde.plasma.systemtray')[0]||add('org.kde.plasma.systemtray');var clock=p.widgets('org.kde.plasma.digitalclock')[0]||add('org.kde.plasma.digitalclock');
var notifications=fluent?(p.widgets('org.kde.plasma.notifications')[0]||add('org.kde.plasma.notifications')):null;
if(fluent){tray.currentConfigGroup=['General'];var hidden=tray.readConfig('hiddenItems',[]);if(hidden.indexOf('org.kde.plasma.notifications')<0)hidden.push('org.kde.plasma.notifications');tray.writeConfig('hiddenItems',hidden);}
function spacer(){var w=add('org.kde.plasma.panelspacer');w.writeConfig('expanding',true);return w;}
var peek=fluent?(p.widgets('org.kde.plasma.showdesktop')[0]||add('org.kde.plasma.showdesktop')):null;
var extras=[];old.forEach(function(w){w.currentConfigGroup=['General'];var popup=w.type==='org.kde.plasma.icon'&&/\/kamakiri-(launcher|dashboard)\.desktop$/.test(w.readConfig('url',''));
if(launcherTypes.indexOf(w.type)>=0||w.type==='org.kde.plasma.panelspacer'||(look!=='caelestia'&&popup))w.remove();else if(w.id!==tasks.id&&w.id!==tray.id&&w.id!==clock.id&&(!peek||w.id!==peek.id)&&(!notifications||w.id!==notifications.id))extras.push(w);});
var order=[];if(look==='fluent11')order.push(spacer());
order.push(start);order.push(tasks);extras.forEach(function(w){order.push(w);});order.push(spacer());order.push(tray);order.push(clock);if(notifications)order.push(notifications);if(peek)order.push(peek);
order.slice().reverse().forEach(function(w){w.index=0;});start.globalShortcut='Alt+F1';
p.alignment=look==='fluent10'?'left':'center';p.offset=0;p.lengthMode='fill';p.hiding='none';print('STUDIO_OK');}catch(e){print('STUDIO_ERROR:'+e);})JS").arg(id).arg(jsonString(look),jsonString(mode),jsonString(owner));
}
void applyLook(const QJsonObject &r,const QString &owner) {
    QString look=r["desktopLook"].toString();if(!desktopLooks().contains(look))bad("Unknown desktop look.");
    if(look!="plasma"&&QStandardPaths::locate(QStandardPaths::GenericDataLocation,"plasma/desktoptheme/kamakiri-"+look+"/metadata.json").isEmpty())bad("Install the desktop-look assets first.");
    if(!r["layoutOnly"].toBool())applyTheme(look=="plasma"?"default":"kamakiri-"+look);
    if(!r["layoutOnly"].toBool()){
    for(const auto &entry:QList<QStringList>{{"kdeglobals","KDE","widgetStyle","Breeze"},{"kdeglobals","Icons","Theme","breeze"},{"kwinrc","org.kde.kdecoration2","library","org.kde.breeze"},{"kwinrc","org.kde.kdecoration2","theme",""},{"kwinrc","org.kde.kdecoration2","BorderSize","None"},{"kwinrc","org.kde.kdecoration2","ButtonsOnLeft",""},{"kwinrc","org.kde.kdecoration2","ButtonsOnRight","IAX"}}){KConfig c(entry[0],KConfig::SimpleConfig);c.group(entry[1]).writeEntry(entry[2],entry[3],KConfig::Notify);if(!c.sync())bad("Cannot apply native desktop styling.");}
    }
    notifyLook();plasmaScript(applyLookScript(r["panel"].toObject()["id"].toInt(),look,r["popupMode"].toString(),owner));
    if(look=="fluent11"||look=="fluent10")plasmaScript(QString(R"JS(try{var p=panelById(%1);var w=p.widgets().sort(function(a,b){return a.index-b.index;});if(w.length<3||w[w.length-3].type!=='org.kde.plasma.digitalclock'||w[w.length-2].type!=='org.kde.plasma.notifications'||w[w.length-1].type!=='org.kde.plasma.showdesktop')throw new Error('Clock and Desktop Peek placement could not be verified');print('STUDIO_OK');}catch(e){print('STUDIO_ERROR:'+e);})JS").arg(r["panel"].toObject()["id"].toInt()));
    bool found=false;for(const auto &p:desktopInventory()["panelWidgets"].toArray())if(p.toObject()["id"]==r["panel"].toObject()["id"])for(const auto &w:p.toObject()["widgets"].toArray())if(w.toObject()["type"]==(look=="plasma"?"org.kde.plasma.kickoff":"org.kamakiri.start")&&w.toObject()["owner"]==owner)found=true;
    if(!found)bad("The new Start menu could not be verified.");
}
QString restoreLookScript(const QJsonObject &saved,const QString &owner) {
    auto p=saved["panel"].toObject();if(p["id"].toInt(-1)<0||!QRegularExpression("^[a-f0-9-]{36}$").match(owner).hasMatch())bad("Invalid desktop look recovery record.");
    return QString(R"JS(try {var old=%1;var owner=%2;var p=panelById(old.id);if(!p&&old.full){p=panels().filter(function(a){a.currentConfigGroup=['General'];return a.readConfig('KamaKiriRecreatedPanel','')===owner+':'+old.id;})[0];if(!p){p=new Panel();p.currentConfigGroup=['General'];p.writeConfig('KamaKiriRecreatedPanel',owner+':'+old.id);}}if(!p)throw new Error('Original panel unavailable');
p.widgets().forEach(function(w){w.currentConfigGroup=['General'];if(w.readConfig('KamaKiriTransaction','')===owner)w.remove();});
function restore(w,node,mapping){var path=node.path.slice();if(path[0]==='Applets'&&mapping[path[1]])path[1]=mapping[path[1]];w.currentConfigGroup=path;Object.keys(node.entries).forEach(function(k){w.writeConfig(k,node.entries[k]);});node.children.forEach(function(c){restore(w,c,mapping);});}
function restoreWidget(w,s){if(!s.config)return;var mapping={};if(s.type==='org.kde.plasma.systemtray'){w.currentConfigGroup=['Applets'];var groups=Array.prototype.slice.call(w.configGroups);var available={};groups.forEach(function(g){w.currentConfigGroup=['Applets',g];available[w.readConfig('plugin','')]=g;});s.config.children.forEach(function(node){if(node.path.length===1&&node.path[0]==='Applets')node.children.forEach(function(child){var plugin=child.entries.plugin;if(!available[plugin])throw new Error('Tray widget unavailable during restoration: '+plugin);mapping[child.path[1]]=available[plugin];});});}restore(w,s.config,mapping);}
var order=[];old.widgets.forEach(function(s){var w=p.widgetById(s.id);if(!w&&(s.launcher||old.full)){w=p.widgets().filter(function(a){a.currentConfigGroup=['General'];return a.readConfig('KamaKiriRestoredFrom','')===owner+':'+s.id;})[0];if(!w){w=p.addWidget(s.type);w.currentConfigGroup=['General'];w.writeConfig('KamaKiriRestoredFrom',owner+':'+s.id);}restoreWidget(w,s);w.globalShortcut=s.shortcut;}else if(w&&(old.full||s.config)){restoreWidget(w,s);w.globalShortcut=s.shortcut;}if(!w)throw new Error('Original taskbar widget unavailable');order.push({w:w,index:s.index});});order.sort(function(a,b){return a.index-b.index;});order.slice().reverse().forEach(function(v){v.w.index=v.index;});
if(old.full){p.screen=old.screen;p.location=old.location;p.height=old.height;p.floating=old.floating;}p.alignment=old.alignment;p.offset=old.offset;p.lengthMode=old.lengthMode;if(old.lengthMode==='custom')p.length=old.length;p.hiding=old.hiding;print(p.id);}catch(e){print('STUDIO_ERROR:'+e);})JS").arg(QString::fromUtf8(QJsonDocument(p).toJson(QJsonDocument::Compact)),jsonString(owner));
}
QJsonArray orderedPanelWidgets(const QJsonArray &widgets) {
    QList<QJsonObject> sorted;for(const auto &w:widgets)sorted.append(w.toObject());
    std::sort(sorted.begin(),sorted.end(),[](const auto &a,const auto &b){return a["index"].toInt()<b["index"].toInt();});
    QJsonArray result;for(const auto &w:sorted)result.append(w);return result;
}
QJsonObject comparablePanelWidgetConfig(QJsonObject node,const QMap<QString,QString> &ids) {
    // Plasma's Icon widget can mirror the same cached launcher path into General.
    // Compare that duplicate once; keep the URL and any differing path significant.
    if(node["path"].toArray().isEmpty()&&node["entries"].toObject().contains("localPath")){
        QJsonValue cached=node["entries"].toObject().value("localPath");auto children=node["children"].toArray();
        for(int i=0;i<children.size();i++){auto child=children[i].toObject();auto entries=child["entries"].toObject();if(child["path"].toArray()==QJsonArray{"General"}&&entries.contains("localPath")&&entries["localPath"]==cached){entries.remove("localPath");child["entries"]=entries;children[i]=child;}}
        node["children"]=children;
    }
    QMap<QString,QString> mapping=ids;
    for(const auto &v:node["children"].toArray()){auto child=v.toObject();auto path=child["path"].toArray();if(path.size()==1&&path[0]=="Applets")for(const auto &w:child["children"].toArray()){auto item=w.toObject();auto parts=item["path"].toArray();if(parts.size()>1)mapping[parts[1].toString()]=item["entries"].toObject()["plugin"].toString();}}
    auto path=node["path"].toArray();
    // Plasma creates configuration-window geometry independently of desktop settings.
    if(path.size()==1&&path[0]=="ConfigDialog"){
        auto entries=node["entries"].toObject();entries.remove("DialogHeight");entries.remove("DialogWidth");node["entries"]=entries;
    }
    if(path.size()>1&&path[0]=="Applets"&&mapping.contains(path[1].toString()))path[1]=mapping[path[1].toString()];
    QJsonObject result;auto entries=node["entries"].toObject();if(!entries.isEmpty())result[QString::fromUtf8(QJsonDocument(path).toJson(QJsonDocument::Compact))]=entries;
    for(const auto &v:node["children"].toArray()){auto child=comparablePanelWidgetConfig(v.toObject(),mapping);for(auto it=child.begin();it!=child.end();++it)result[it.key()]=it.value();}
    return result;
}
void restoreLook(const QJsonObject &saved,const QString &owner) {
    if(saved.isEmpty())bad("Desktop look snapshot is missing.");
    QString theme="default";
    for(const auto &v:saved["settings"].toArray()){auto e=v.toObject();if(e["file"]=="plasmarc"&&e["key"]=="name"&&!e["value"].toString().isEmpty())theme=e["value"].toString();}
    applyTheme(theme);
    for(const auto &v:saved["settings"].toArray()){auto e=v.toObject();QStringList identity{e["file"].toString(),e["group"].toString(),e["key"].toString()};if(!settingsKeys.contains(identity))bad("Unexpected desktop styling recovery key.");KConfig c(identity[0],KConfig::SimpleConfig);auto g=c.group(identity[1]);if(e["present"].toBool())g.writeEntry(identity[2],e["value"].toString(),KConfig::Notify);else g.deleteEntry(identity[2],KConfig::Notify);if(!c.sync())bad("Cannot restore desktop styling.");}
    notifyLook();if(!saved.contains("panel")){if(snapshotDesktopStyling()["settings"]!=saved["settings"])bad("Desktop styling restoration could not be verified.");return;}QString restoredId;
    for(int attempt=0;;attempt++){
        try{restoredId=plasmaScript(restoreLookScript(saved,owner)).trimmed();break;}
        catch(const std::exception &e){if(attempt>=29||!saved["panel"].toObject()["full"].toBool()||!QString::fromUtf8(e.what()).contains("Tray widget unavailable"))throw;QThread::msleep(100);}
    }
    if(saved["panel"].toObject()["full"].toBool()){QThread::msleep(200);restoredId=plasmaScript(restoreLookScript(saved,owner)).trimmed();QThread::msleep(200);}
    bool valid=false;int panelId=restoredId.toInt(&valid);if(!valid)bad("Cannot identify the restored panel.");
    auto restored=snapshotLook(panelId,saved["panel"].toObject()["full"].toBool());
    if(restored["settings"]!=saved["settings"])bad("Desktop styling restoration could not be verified.");
    auto oldPanel=saved["panel"].toObject(),newPanel=restored["panel"].toObject();

    auto oldWidgets=orderedPanelWidgets(oldPanel["widgets"].toArray()),newWidgets=orderedPanelWidgets(newPanel["widgets"].toArray());
    if(oldWidgets.size()!=newWidgets.size())bad("Original taskbar widget count differs after restoration.");
    for(int i=0;i<oldWidgets.size();i++){
        auto a=oldWidgets[i].toObject(),b=newWidgets[i].toObject();
        // Older snapshots did not capture tray/spacer settings; retain that contract.
        if(a["type"]=="org.kde.plasma.systemtray"&&a["config"].isObject()&&a["config"].toObject()["path"].toArray().isEmpty()&&!oldPanel["full"].toBool())for(const auto &w:snapshotLook(panelId,true)["panel"].toObject()["widgets"].toArray())if(w.toObject()["id"]==b["id"])b["config"]=w.toObject()["config"];
        bool sameConfig=!a["config"].isObject()||comparablePanelWidgetConfig(a["config"].toObject())==comparablePanelWidgetConfig(b["config"].toObject());
        bool sameIdentity=oldPanel["full"].toBool()||a["launcher"].toBool()||a["id"]==b["id"];
        if(a["type"]!=b["type"]||!sameConfig||a["shortcut"]!=b["shortcut"]||a["index"]!=b["index"]||!sameIdentity)
            bad("Original taskbar settings could not be verified for "+a["type"].toString()+": "+(!sameConfig?"configuration differs":a["shortcut"]!=b["shortcut"]?"shortcut differs":"widget position or identity differs"));
    }
    if(oldPanel["full"].toBool())for(const auto &key:{"screen","location","height","floating"})if(oldPanel[key]!=newPanel[key])bad("Restored panel geometry differs.");
    for(const auto &key:{"alignment","lengthMode","offset","hiding"})if(oldPanel[key]!=newPanel[key])bad("Original taskbar placement could not be verified.");
}
}
