// SPDX-License-Identifier: MIT
#include "core.h"
#include "input_monitor.h"
#include <KConfig>
#include <KConfigGroup>
#include <KIdleTime>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QUuid>
#include <QSettings>
#include <QUrl>
#include <QImageReader>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <signal.h>

namespace Studio {
static void fail(const QString &s) { throw std::runtime_error(s.toStdString()); }
QString stateRoot() {
    QString root = QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + "/kamakiri-studio";
    if (!QDir().mkpath(root)) fail("Cannot create the private recovery folder.");
    struct stat st;
    if (lstat(QFile::encodeName(root).constData(), &st) != 0 || !S_ISDIR(st.st_mode) || st.st_uid != getuid())
        fail("Recovery folder must be a real directory owned by you.");
    if (chmod(QFile::encodeName(root).constData(), 0700) != 0) fail("Cannot protect recovery folder.");
    return root;
}
QString configFile() { return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/kdeglobals"; }
QString jsonString(const QString &s) {
    QByteArray a = QJsonDocument(QJsonArray{s}).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(a.mid(1, a.size()-2));
}
QJsonObject readJson(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    if (f.size() > 1024*1024) fail("Recovery record is unexpectedly large.");
    QJsonParseError e;
    auto doc = QJsonDocument::fromJson(f.readAll(), &e);
    if (e.error != QJsonParseError::NoError || !doc.isObject()) fail("Recovery record is invalid.");
    return doc.object();
}
void writeJson(const QString &path, const QJsonObject &value) {
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) fail("Cannot write recovery record.");
    f.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    QByteArray bytes = QJsonDocument(value).toJson();
    if (f.write(bytes) != bytes.size() || !f.commit()) fail("Cannot save recovery record.");
}
bool unfinished(const QString &s) { return s == "applying" || s == "pending" || s == "restoring" || s == "recovery-needed"; }
QString plasmaScript(const QString &script) {
    auto m = QDBusMessage::createMethodCall("org.kde.plasmashell", "/PlasmaShell", "org.kde.PlasmaShell", "evaluateScript");
    m << script;
    auto reply = QDBusConnection::sessionBus().call(m, QDBus::Block, 5000);
    if (reply.type() == QDBusMessage::ErrorMessage) fail("Plasma did not respond: " + reply.errorMessage());
    QString output = reply.arguments().value(0).toString();
    if (output.contains("STUDIO_ERROR:")) fail(output);
    return output;
}
QJsonArray panels() {
    QString out = plasmaScript("print(JSON.stringify(panels().map(function(p){return {id:p.id,location:p.location,height:p.height,floating:p.floating,screen:p.screen};})))");
    QJsonParseError e;
    auto doc = QJsonDocument::fromJson(out.trimmed().toUtf8(), &e);
    if (e.error != QJsonParseError::NoError || !doc.isArray()) fail("Cannot read the Plasma panel layout safely.");
    return doc.array();
}
QString panelScript(const QJsonObject &p) {
    int id = p["id"].toInt(-1), height = p["height"].toInt(-1);
    QString edge = p["location"].toString();
    if (id < 0 || height < 16 || height > 256 || !QStringList{"top","bottom","left","right"}.contains(edge) || !p["floating"].isBool())
        fail("Panel settings are invalid.");
    return QString("try {var p=panelById(%1); if(!p) throw new Error('Selected panel no longer exists'); p.location=%2; p.height=%3; p.floating=%4; print('STUDIO_OK');} catch(e){print('STUDIO_ERROR:'+e);}")
        .arg(id).arg(jsonString(edge)).arg(height).arg(p["floating"].toBool()?"true":"false");
}
bool panelMatchesState(const QJsonObject &actual,const QJsonObject &expected) {
    for(const auto &key:{"id","location","height","floating"})if(actual[key]!=expected[key])return false;
    return !expected.contains("screen")||actual["screen"]==expected["screen"];
}
QJsonArray trialPanels(const QJsonObject &request) {
    if(!request["changePanel"].toBool())return {};
    return request.contains("panels")?request["panels"].toArray():QJsonArray{request["panel"]};
}
void validateRequest(const QJsonObject &r) {
    if(r["panelPopupActions"].toBool()&&r["desktopLook"]!="caelestia")fail("KamaKiri popup buttons require Kamakiri style.");
    if(!QStringList{"single","all","span"}.contains(r["wallpaperScope"].toString("single")))fail("Unknown wallpaper display mode.");
    if(!r["desktopLook"].toString().isEmpty()){if(!desktopLooks().contains(r["desktopLook"].toString())||!r["changePanel"].toBool())fail("Choose a panel for the desktop look.");}

    if (!QStringList{"caelestia","ryoku","breeze","windows"}.contains(r["preset"].toString()) && r["preset"]!="wallpaper" && omarchyPalette(r["preset"].toString()).isEmpty()) fail("Unknown preset.");
    if (!QRegularExpression("^#[0-9A-Fa-f]{6}$").match(r["accent"].toString()).hasMatch()) fail("Accent must be a six-digit hex color.");
    if(r.contains("widgets")&&!r["widgets"].isArray())fail("Widget choices must be a list.");
    if(r["changePopupLook"].toBool()&&(!QStringList{"rounded","fluent"}.contains(r["popupStyle"].toString())||!QStringList{"desktop","light","dark"}.contains(r["popupMode"].toString())))fail("Unknown popup look.");
    if (r["changePanel"].toBool()||r["panelPopupActions"].toBool()) {for(const auto &p:trialPanels(r))panelScript(p.toObject());}
    for(const auto &key:{"panels","createPanels","removePanelIds","wallpaperDesktopIds"})if(r.contains(key)&&(!r[key].isArray()||r[key].toArray().size()>32))fail("Invalid monitor or panel selection.");
    QSet<int> ids;for(const auto &v:trialPanels(r)){auto p=v.toObject();panelScript(p);int id=p["id"].toInt(-1);if(ids.contains(id))fail("A panel was selected twice.");ids.insert(id);}
    for(const auto &id:r["removePanelIds"].toArray()){if(!id.isDouble()||id.toInt(-1)<0||ids.contains(id.toInt()))fail("Invalid panel removal choice.");ids.insert(id.toInt());}
    for(const auto &v:r["createPanels"].toArray()){auto p=v.toObject();if(!p["screen"].isDouble()||p["screen"].toInt(-1)<0)fail("Invalid new panel monitor.");p["id"]=0;panelScript(p);}
    if(r.contains("wallpaperDesktopIds")&&!r["wallpaperDesktopIds"].isArray())fail("Invalid monitor selection.");
    if(r["changeWallpaper"].toBool()||r["preset"]=="wallpaper") loadWallpaper(r["wallpaperPath"].toString());
    if(r["changeWallpaper"].toBool()||!r["widgets"].toArray().isEmpty()) {
        if(!r["desktopId"].isDouble()||r["desktopId"].toInt(-1)<0)fail("Choose a desktop target.");
        wallpaperScript(r["desktopId"].toInt(),QString());
        addWidgetsScript(r["desktopId"].toInt(),r["widgets"].toArray(),"12345678-1234-1234-1234-123456789abc");
    }
}
QJsonArray sessions(const QStringList &given) {
    QStringList roots = given;
    if (roots.isEmpty()) {
        for (const QString &p : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
            roots << p+"/wayland-sessions" << p+"/xsessions";
        }
    }
    QJsonArray result;
    QSet<QString> seen;
    for (const auto &root : roots) {
        QDir d(root);
        for (const auto &name : d.entryList({"*.desktop"}, QDir::Files, QDir::Name)) {
            QString type = root.endsWith("wayland-sessions") ? "Wayland" : "X11";
            QString identity = type + ":" + name;
            if (seen.contains(identity)) continue;
            seen.insert(identity);
            KConfig c(d.filePath(name), KConfig::SimpleConfig);
            KConfigGroup g(&c, "Desktop Entry");
            if (g.readEntry("Hidden", false) || g.readEntry("NoDisplay", false)) continue;
            QString exec = g.readEntry("TryExec", QString());
            bool usable = exec.isEmpty() || (exec.startsWith('/') ? QFileInfo(exec).isExecutable() : !QStandardPaths::findExecutable(exec).isEmpty());
            result.append(QJsonObject{{"name",g.readEntry("Name", name)}, {"file",d.filePath(name)}, {"type",type}, {"available",usable}});
        }
    }
    return result;
}
static bool managedGroup(const QString &s) { return s.startsWith("Colors:") || s.startsWith("ColorEffects:"); }
void restoreColors(const QString &backup, const QString &destination) {
    if (!QFileInfo::exists(backup)) fail("The color recovery snapshot is missing.");
    KConfig before(backup, KConfig::SimpleConfig), now(destination, KConfig::SimpleConfig);
    QSet<QString> groups;
    for (const auto &g : before.groupList()) if (managedGroup(g)) groups.insert(g);
    for (const auto &g : now.groupList()) if (managedGroup(g)) groups.insert(g);
    for (const auto &g : groups) {
        now.deleteGroup(g);
        if (before.hasGroup(g)) {
            KConfigGroup target(&now,g);
            before.group(g).copyTo(&target);
        }
    }
    const QMap<QString, QStringList> keys{
        {"General", {"ColorScheme","ColorSchemeHash","AccentColor","LastUsedCustomAccentColor"}},
        {"KDE", {"contrast","frameContrast"}},
        {"WM", {"activeBackground","activeForeground","inactiveBackground","inactiveForeground","activeBlend","inactiveBlend"}}
    };
    for (auto it = keys.begin(); it != keys.end(); ++it) {
        auto source = before.group(it.key()), target = now.group(it.key());
        for (const auto &key : it.value()) {
            if (source.hasKey(key)) target.writeEntry(key, source.readEntry(key), KConfig::Notify);
            else target.deleteEntry(key, KConfig::Notify);
        }
    }
    if (!now.sync()) fail("Cannot restore the previous color settings.");
}
static QColor mix(const QColor &a, const QColor &b, double f) {
    return QColor::fromRgbF(a.redF()*(1-f)+b.redF()*f,a.greenF()*(1-f)+b.greenF()*f,a.blueF()*(1-f)+b.blueF()*f);
}
QString makeScheme(const QJsonObject &r, const QString &id) {
    if (!QRegularExpression("^[a-f0-9-]{36}$").match(id).hasMatch()) fail("Invalid transaction ID.");
    auto palette = desktopPalette(r["preset"].toString(),r["desktopLook"].toString().isEmpty()?"desktop":r["popupMode"].toString("desktop"));
    if(r["preset"]=="wallpaper") palette=wallpaperPalette(r["wallpaperPath"].toString());
    bool light = palette.isEmpty() ? r["light"].toBool() : palette["mode"] == "light";
    QString base = QStandardPaths::locate(QStandardPaths::GenericDataLocation, light?"color-schemes/BreezeLight.colors":"color-schemes/BreezeDark.colors");
    if (base.isEmpty()) fail("The built-in Breeze color scheme is not installed.");
    QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/color-schemes";
    if (!QDir().mkpath(dir)) fail("Cannot create your color scheme folder.");
    QString path = dir + "/KamaKiriStudio-"+id+".colors";
    if (!QFile::copy(base, path)) fail("Cannot create the trial color scheme.");
    KConfig scheme(path, KConfig::SimpleConfig);
    auto rgb = [](const QColor &c) { return QString("%1,%2,%3").arg(c.red()).arg(c.green()).arg(c.blue()); };
    QColor accent(r["accent"].toString());
    bool caelestia = r["preset"] == "caelestia";
    QColor bg = light ? QColor(caelestia?"#f5f0fb":"#f3f4f5") : QColor(caelestia?"#1c1927":"#1b1d23");
    if (r["preset"] != "breeze") {
        for (const auto &group : {"Colors:Window","Colors:View","Colors:Button","Colors:Header","Colors:Tooltip","Colors:Complementary"}) {
            auto g = scheme.group(group);
            g.writeEntry("BackgroundNormal",rgb( bg));
            g.writeEntry("BackgroundAlternate",rgb( mix(bg,accent,0.08)));
            for (const auto &key : {"DecorationFocus","DecorationHover","ForegroundActive","ForegroundLink"}) g.writeEntry(key,rgb(accent));
            if (g.hasGroup("Inactive")) {
                auto inactive = g.group("Inactive");
                inactive.writeEntry("BackgroundNormal",rgb(bg));
                inactive.writeEntry("BackgroundAlternate",rgb(mix(bg,accent,0.05)));
            }
        }
        auto sel = scheme.group("Colors:Selection");
        QColor selected = mix(bg,accent,0.55);
        sel.writeEntry("BackgroundNormal",rgb(selected));
        sel.writeEntry("BackgroundAlternate",rgb(selected));
        QColor foreground = selected.lightnessF()>0.6 ? QColor("#15161b") : QColor("#ffffff");
        sel.writeEntry("ForegroundNormal",rgb(foreground));
        sel.writeEntry("ForegroundInactive",rgb(foreground));
    }
    if (!palette.isEmpty()) {
        auto color = [&](const char *key) { return QColor(palette[key].toString()); };
        for (const auto &group : {"Colors:Window","Colors:View","Colors:Button","Colors:Header","Colors:Tooltip","Colors:Complementary","Colors:Selection"}) {
            auto g=scheme.group(group);
            g.writeEntry("BackgroundNormal",rgb(color(QString(group)=="Colors:Selection"?"selection":QString(group)=="Colors:View"?"dark_background":"background")));
            g.writeEntry("BackgroundAlternate",rgb(color("lighter_background")));
            g.writeEntry("ForegroundNormal",rgb(color("foreground")));
            g.writeEntry("ForegroundInactive",rgb(color("dark_foreground")));
            g.writeEntry("ForegroundActive",rgb(accent));
            g.writeEntry("ForegroundLink",rgb(accent));
            g.writeEntry("ForegroundVisited",rgb(color("magenta")));
            g.writeEntry("ForegroundNegative",rgb(color("red")));
            g.writeEntry("ForegroundNeutral",rgb(color("orange")));
            g.writeEntry("ForegroundPositive",rgb(color("green")));
            g.writeEntry("DecorationFocus",rgb(accent)); g.writeEntry("DecorationHover",rgb(accent));
            if (g.hasGroup("Inactive")) {
                auto inactive=g.group("Inactive");
                inactive.writeEntry("BackgroundNormal",rgb(color("background")));
                inactive.writeEntry("BackgroundAlternate",rgb(color("lighter_background")));
                inactive.writeEntry("ForegroundNormal",rgb(color("foreground")));
            }
        }
        auto wm=scheme.group("WM");
        wm.writeEntry("activeBackground",rgb(color("background"))); wm.writeEntry("inactiveBackground",rgb(color("dark_background")));
        wm.writeEntry("activeForeground",rgb(color("foreground"))); wm.writeEntry("inactiveForeground",rgb(color("dark_foreground")));
    }
    auto general = scheme.group("General");
    general.writeEntry("Name", "KamaKiriStudio · " + r["preset"].toString());
    if (!scheme.sync()) fail("Cannot save the trial color scheme.");
    return path;
}
void notifyPalette() {
    auto m = QDBusMessage::createSignal("/KGlobalSettings", "org.kde.KGlobalSettings", "notifyChange");
    m << 0 << 0;
    QDBusConnection::sessionBus().send(m);
}
QJsonObject inspect() {
    QJsonObject o{{"sessions",sessions()}, {"desktop",qEnvironmentVariable("XDG_CURRENT_DESKTOP")}, {"sessionType",qEnvironmentVariable("XDG_SESSION_TYPE")}};
    try { o["desktopTools"]=desktopInventory(); o["panels"] = panels(); o["plasmaAvailable"] = true; }
    catch (const std::exception &e) { o["plasmaAvailable"] = false; o["error"] = QString::fromUtf8(e.what()); }
    o["colorTool"] = QStandardPaths::findExecutable("plasma-apply-colorscheme");
    if(qEnvironmentVariable("XDG_SESSION_TYPE")=="wayland") {
        InputMonitor monitor;
        o["globalInputReady"]=monitor.prepare();
    } else o["globalInputReady"]=qEnvironmentVariable("XDG_SESSION_TYPE")=="x11";
    return o;
}
static void copyBackup(const QString &from, const QString &to) {
    QFile source(from);
    QByteArray data;
    if (source.exists()) {
        if (!source.open(QIODevice::ReadOnly)) fail("Cannot read color settings for rollback.");
        data = source.readAll();
    }
    QSaveFile dest(to);
    if (!dest.open(QIODevice::WriteOnly)) fail("Cannot save the color backup.");
    dest.setPermissions(QFile::ReadOwner|QFile::WriteOwner);
    if (dest.write(data)!=data.size() || !dest.commit()) fail("Cannot save the color backup.");
}
static void restoreSnapshot(const QString &dir, bool demo) {
    auto snapshot = readJson(dir+"/snapshot.json");
    if (snapshot.isEmpty()) fail("Recovery snapshot is missing.");
    if (!demo) {
        if(snapshot["changeColors"].toBool(true)) {restoreColors(dir+"/colors-before.ini", configFile());notifyPalette();}
        if(snapshot["changePopupLook"].toBool()) {
            QSettings settings;auto old=snapshot["popupBefore"].toObject();for(const auto &key:{"popup/style","popup/mode","desktop/look"}){auto entry=old[key].toObject();if(entry["present"].toBool())settings.setValue(key,entry["value"].toVariant());else settings.remove(key);}settings.sync();if(settings.status()!=QSettings::NoError)fail("Popup look restoration failed.");
        }
        QString owner=QFileInfo(dir).fileName();
        plasmaScript(QString("panels().forEach(function(p){p.currentConfigGroup=['General'];if(p.readConfig('KamaKiriPanelOwner','')===%1)p.remove();});").arg(jsonString(owner)));
        if(snapshot.contains("desktopStylingBefore"))restoreLook(snapshot["desktopStylingBefore"].toObject(),owner);
        auto looks=snapshot["desktopLooksBefore"].toArray();if(looks.isEmpty()&&snapshot["changeDesktopLook"].toBool()&&snapshot.contains("desktopLookBefore"))looks.append(snapshot["desktopLookBefore"]);
        for(const auto &look:looks)restoreLook(look.toObject(),owner);
        for(const auto &look:snapshot["removedPanelsBefore"].toArray())restoreLook(look.toObject(),owner);
        if(snapshot["changePanel"].toBool()){
            auto before=snapshot["panelsBefore"].toArray();if(before.isEmpty()&&snapshot.contains("panel"))before.append(snapshot["panel"]);
            for(const auto &v:before){plasmaScript(panelScript(v.toObject()));bool restored=false;for(const auto &p:panels())if(panelMatchesState(p.toObject(),v.toObject()))restored=true;if(!restored)fail("Panel restoration could not be verified. Recovery data has been preserved.");}
        }
        if(snapshot["changeWallpaper"].toBool()) {
            auto before=snapshot["wallpaperBefore"].toArray();if(before.isEmpty())before.append(snapshot["desktop"]);
            for(const auto &entry:before){auto old=entry.toObject();plasmaScript(wallpaperScript(old["id"].toInt(),old["image"].toString()));bool ok=false;for(const auto &d:desktopInventory()["desktops"].toArray())if(d.toObject()["id"]==old["id"]&&d.toObject()["image"]==old["image"])ok=true;if(!ok)fail("Wallpaper restoration could not be verified.");}
            QDir generated(dir+"/wallpapers");if(generated.exists())generated.removeRecursively();
        }
        if(snapshot["changeWidgets"].toBool()) {
            QString owner=QFileInfo(dir).fileName();plasmaScript(removeTrialWidgetsScript(owner));
            auto recovered=desktopInventory();for(const auto &key:{"desktops","panelWidgets"})for(const auto &d:recovered[key].toArray())for(const auto &w:d.toObject()["widgets"].toArray())if(w.toObject()["owner"]==owner)fail("Widget restoration could not be verified.");
        }
        // This is always a freshly generated file with an internal UUID, never a supplied path.
        QString id = QFileInfo(dir).fileName();
        QString own = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/color-schemes/KamaKiriStudio-"+id+".colors";
        QFile::remove(own);
    }
}
Worker::Worker(const QString &transaction, bool simulate, QObject *parent):QObject(parent),dir(transaction),demo(simulate) {
    poll.setInterval(100);
    connect(&poll,&QTimer::timeout,this,[this]{tick();});
}
void Worker::setState(const QString &s, const QString &message) {
    status["state"]=s; status["message"]=message; status["interacted"]=policy.interacted;
    writeJson(dir+"/status.json",status);
}
void Worker::recordInput() {
    if (!armed || policy.interacted) return;
    policy.input();
    if(inputMonitor) inputMonitor->stop();
    else if(!demo) KIdleTime::instance()->stopCatchingResumeEvent();
    setState("pending","Take your time testing. Choose Yes to keep or No to restore.");
}
void Worker::start() {
    lifeClock.start();
    try {
        request=readJson(dir+"/request.json");
        validateRequest(request);
        setState("applying","Saving your settings and applying the trial…");
        if (!demo) {
            if (qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains("KDE",Qt::CaseInsensitive)==false)
                fail("Appearance changes require a KDE Plasma session.");
            if(qEnvironmentVariable("XDG_SESSION_TYPE")=="wayland") {
                inputMonitor=new InputMonitor(this);
                if(!inputMonitor->prepare()) fail("This session cannot provide inhibitor-independent input detection. No changes were applied.");
                connect(inputMonitor,&InputMonitor::activity,this,[this]{recordInput();});
                connect(inputMonitor,&InputMonitor::unavailable,this,[this]{if(armed)rollback("Desktop input monitoring became unavailable.");});
            }
            auto ps=panels();
            QJsonArray before;
            for(const auto &target:trialPanels(request)){
                bool found=false;for(const auto &p:ps)if(p.toObject()["id"]==target.toObject()["id"]){before.append(p);found=true;}
                if(!found)fail("A selected panel is no longer available.");
            }
            snapshot["panelsBefore"]=before;if(!before.isEmpty())snapshot["panel"]=before.first();
            QJsonArray removed;
            const int primaryForRemoval=request["changePanel"].toBool()&&!request["removePanelIds"].toArray().isEmpty()?primaryDesktopScreen(desktopInventory()["desktops"].toArray()):-1;
            if(request["changePanel"].toBool())for(const auto &id:request["removePanelIds"].toArray()){
                QJsonObject old;for(const auto &p:ps)if(p.toObject()["id"]==id)old=p.toObject();
                if(old.isEmpty())fail("The panel selected for removal is unavailable.");
                if(old["screen"].toInt(0)==primaryForRemoval)fail("The primary monitor must retain its panel.");
                auto saved=snapshotLook(id.toInt(),true);saved["recreate"]=true;removed.append(saved);
            }
            snapshot["removedPanelsBefore"]=removed;
            if(request["changePanel"].toBool())for(const auto &v:request["createPanels"].toArray()){auto panel=v.toObject();panel["id"]=0;panelScript(panel);bool found=false;for(const auto &d:desktopInventory()["desktops"].toArray())if(d.toObject()["screen"]==panel["screen"]&&d.toObject()["active"].toBool(true))found=true;if(!found)fail("A selected monitor disconnected.");}
            if(request["changeWallpaper"].toBool()||!request["widgets"].toArray().isEmpty()) {
                auto desktopData=desktopInventory();bool found=false;
                for(const auto &v:desktopData["desktops"].toArray())if(v.toObject()["id"]==request["desktopId"]){snapshot["desktop"]=v;found=true;}
                if(!found)fail("The selected desktop is unavailable.");
                if(request["changeWallpaper"].toBool())snapshot["wallpaperBefore"]=wallpaperTargets(desktopData["desktops"].toArray(),request["desktopId"].toInt(),request["wallpaperScope"].toString("single"),request["wallpaperDesktopIds"].toArray());
                for(const auto &t:request["widgets"].toArray())if(!desktopData["types"].toArray().contains(t))fail("A selected widget is no longer installed.");
            }
            if(request["changeColors"].toBool(true))copyBackup(configFile(),dir+"/colors-before.ini");
            if(request["panelPopupActions"].toBool()) {
                bool found=false;for(const auto &p:ps)if(p.toObject()["id"]==request["panel"].toObject()["id"])found=true;
                if(!found)fail("The popup panel target is unavailable.");
                panelPopupActionsScript(request["panel"].toObject()["id"].toInt(),QFileInfo(dir).fileName());
            }
            if(!request["desktopLook"].toString().isEmpty()){snapshot["desktopStylingBefore"]=snapshotDesktopStyling();QJsonArray looks;for(const auto &p:trialPanels(request))looks.append(snapshotLook(p.toObject()["id"].toInt()));snapshot["desktopLooksBefore"]=looks;}
            if(request["changePopupLook"].toBool()) {QSettings settings;QJsonObject before;for(const auto &key:{"popup/style","popup/mode","desktop/look"})before[key]=QJsonObject{{"present",settings.contains(key)},{"value",QJsonValue::fromVariant(settings.value(key))}};snapshot["popupBefore"]=before;}
        }
        snapshot["changeDesktopLook"]=!request["desktopLook"].toString().isEmpty();
        snapshot["changePanel"]=request["changePanel"].toBool();
        snapshot["changeWallpaper"]=request["changeWallpaper"].toBool();
        snapshot["changeWidgets"]=!request["widgets"].toArray().isEmpty()||request["panelPopupActions"].toBool()||!request["desktopLook"].toString().isEmpty();
        snapshot["changeColors"]=request["changeColors"].toBool(true);
        snapshot["changePopupLook"]=request["changePopupLook"].toBool();
        writeJson(dir+"/snapshot.json",snapshot);
        if(!demo&&request["changeColors"].toBool(true)) {
            QString scheme=makeScheme(request,QFileInfo(dir).fileName());
            QString tool=QStandardPaths::findExecutable("plasma-apply-colorscheme");
            if(tool.isEmpty()) fail("KDE's color application tool is missing.");
            // Existing user accent overrides schemes, so set it before applying the new palette.
            KConfig global(configFile(),KConfig::SimpleConfig);
            QColor trialAccent(request["accent"].toString());
            global.group("General").writeEntry("AccentColor",QString("%1,%2,%3").arg(trialAccent.red()).arg(trialAccent.green()).arg(trialAccent.blue()),KConfig::Notify);
            if(!global.sync()) fail("Cannot save the trial accent.");
            QProcess apply;
            const pid_t ownerPid=getpid();
            apply.setChildProcessModifier([ownerPid]{
                // A crashed watchdog must not leave a color tool applying settings after recovery.
                if(prctl(PR_SET_PDEATHSIG,SIGKILL)!=0||getppid()!=ownerPid)_exit(127);
            });
            apply.start(tool,{QFileInfo(scheme).completeBaseName()});
            if(!apply.waitForFinished(8000)) {apply.kill(); apply.waitForFinished(); fail("KDE color application timed out.");}
            if(apply.exitStatus()!=QProcess::NormalExit || apply.exitCode()!=0) fail("KDE refused the color scheme: "+QString::fromUtf8(apply.readAllStandardError()));
            KConfig check(configFile(),KConfig::SimpleConfig);
            if(check.group("General").readEntry("ColorScheme",QString())!=QFileInfo(scheme).completeBaseName()) fail("Cannot verify that the trial palette was applied.");
        }
        if(!demo){
            for(const auto &target:trialPanels(request)){
                plasmaScript(panelScript(target.toObject()));bool ok=false;for(const auto &p:panels())if(panelMatchesState(p.toObject(),target.toObject()))ok=true;if(!ok)fail("Panel placement could not be verified.");
                if(!request["desktopLook"].toString().isEmpty()){auto r=request;r["panel"]=target;applyLook(r,QFileInfo(dir).fileName());}
            }
            if(request["changePanel"].toBool()){
                for(const auto &id:request["removePanelIds"].toArray())plasmaScript(QString("var p=panelById(%1);if(p)p.remove();").arg(id.toInt()));
                for(const auto &v:request["createPanels"].toArray()){
                    auto p=v.toObject();QString owner=QFileInfo(dir).fileName();
                    QString script=QString("var p=new Panel();p.currentConfigGroup=['General'];p.writeConfig('KamaKiriPanelOwner',%1);p.screen=%2;print(p.id);").arg(jsonString(owner)).arg(p["screen"].toInt());
                    bool valid=false;int id=plasmaScript(script).trimmed().toInt(&valid);if(!valid)fail("Cannot create a panel.");p["id"]=id;plasmaScript(panelScript(p));auto r=request;r["panel"]=p;if(r["desktopLook"].toString().isEmpty()){r["desktopLook"]="plasma";r["layoutOnly"]=true;}applyLook(r,owner);
                }
            }
            if(request["panelPopupActions"].toBool())plasmaScript(panelPopupActionsScript(request["panel"].toObject()["id"].toInt(),QFileInfo(dir).fileName()));
            if(request["changePopupLook"].toBool()) {QSettings settings;settings.setValue("desktop/look",request["desktopLook"].toString());settings.setValue("popup/style",request["popupStyle"].toString());settings.setValue("popup/mode",request["popupMode"].toString());settings.sync();if(settings.status()!=QSettings::NoError)fail("Cannot save popup look.");}
        }
        if(!demo && request["changeWallpaper"].toBool()) {
            auto targets=snapshot["wallpaperBefore"].toArray();QStringList images;
            if(request["wallpaperScope"]=="span"){
                QImageReader reader(request["wallpaperPath"].toString());reader.setAutoTransform(true);auto source=reader.read();if(source.isNull())fail("Cannot decode the wallpaper for spanning.");
                QList<QRect> geometry;for(const auto &v:targets){auto g=v.toObject()["geometry"].toObject();geometry.append(QRect(g["x"].toInt(),g["y"].toInt(),g["width"].toInt(),g["height"].toInt()));}
                auto crops=spanWallpaper(source,geometry);QString folder=dir+"/wallpapers";if(!QDir().mkpath(folder))fail("Cannot save split wallpaper images.");
                for(int i=0;i<crops.size();i++){QString path=folder+"/screen-"+QString::number(i)+".png";if(!crops[i].save(path))fail("Cannot save a split wallpaper image.");QFile::setPermissions(path,QFile::ReadOwner|QFile::WriteOwner);images.append(path);}
            }else for(int i=0;i<targets.size();i++)images.append(request["wallpaperPath"].toString());
            for(int i=0;i<targets.size();i++){int id=targets[i].toObject()["id"].toInt();QString image=QUrl::fromLocalFile(images[i]).toString();plasmaScript(wallpaperScript(id,image));bool ok=false;for(const auto &d:desktopInventory()["desktops"].toArray())if(d.toObject()["id"].toInt()==id&&d.toObject()["image"]==image)ok=true;if(!ok)fail("Wallpaper application could not be verified.");}
        }
        if(!demo && !request["widgets"].toArray().isEmpty()) {
            plasmaScript(addWidgetsScript(request["desktopId"].toInt(),request["widgets"].toArray(),QFileInfo(dir).fileName()));
            auto desktopData=desktopInventory();QJsonArray actual;
            for(const auto &d:desktopData["desktops"].toArray())if(d.toObject()["id"]==request["desktopId"])for(const auto &w:d.toObject()["widgets"].toArray())actual.append(w.toObject()["type"]);
            for(const auto &t:request["widgets"].toArray())if(!actual.contains(t))fail("Widget application could not be verified.");
        }
        // Arm AFTER application finishes. The click that initiated Apply is not trial input.
        trialClock.start();
        lastHeartbeat=lifeClock.elapsed();
        lastHeartbeatValue=readJson(dir+"/command.json")["heartbeat"].toDouble();
        armed=true;
        if(!demo) {
            if(inputMonitor) {
                if(!inputMonitor->arm()) fail("Cannot arm desktop-wide input detection.");
            } else {
                auto idle=KIdleTime::instance();
                connect(idle,&KIdleTime::resumingFromIdle,this,[this]{recordInput();});
                idle->catchNextResumeEvent();
            }
        }
        setState("pending","Keep these changes? Try them, then choose Yes or No.");
        poll.start();
    } catch(const std::exception &e) {
        if(QFileInfo::exists(dir+"/snapshot.json")) rollback(QString::fromUtf8(e.what()));
        else {setState("failed",QString::fromUtf8(e.what())); QCoreApplication::quit();}
    }
}
void Worker::tick() {
    try {
        auto c=readJson(dir+"/command.json");
        double heartbeat=c["heartbeat"].toDouble();
        if(heartbeat>lastHeartbeatValue) {lastHeartbeat=lifeClock.elapsed();lastHeartbeatValue=heartbeat;}
        bool ownerAlive=lifeClock.elapsed()-lastHeartbeat<4000;
        if(c["decision"]=="no") policy.rejected=true;
        if(c["input"].toBool()) recordInput();
        // Revert wins if the UI died. A stale confirmation cannot keep an orphan trial.
        if(!ownerAlive || policy.rejected) {rollback(ownerAlive?"You chose No.":"The manager closed or stopped responding.");return;}
        if(c["decision"]=="yes") {
            policy.accepted=true;
            armed=false;
            setState("kept", "Changes kept.");
            poll.stop(); QCoreApplication::quit(); return;
        }
        if(policy.shouldRevert(trialClock.elapsed(),ownerAlive)) {rollback("No input was detected during the first 15 seconds.");return;}
        if(!policy.interacted){int remaining=qMax(0,15-int(trialClock.elapsed()/1000));if(status["remainingSeconds"].toInt(-1)!=remaining){status["remainingSeconds"]=remaining;setState("pending","Keep these changes? Try them, then choose Yes or No.");}}
    } catch(const std::exception &e) {rollback(QString::fromUtf8(e.what()));}
}
void Worker::rollback(const QString &reason) {
    if(restoring) return;
    restoring=true; armed=false; poll.stop();
    try {
        setState("restoring","Restoring your previous settings…");
        restoreSnapshot(dir,demo);
        setState("reverted","Previous settings restored. "+reason);
    } catch(const std::exception &e) {
        setState("recovery-needed", "Automatic restoration needs attention: "+QString::fromUtf8(e.what()));
    }
    QCoreApplication::quit();
}
void recover(const QString &dir,bool demo) {
    try {restoreSnapshot(dir,demo);}catch(const std::exception &e){writeJson(dir+"/status.json",{{"state","recovery-needed"},{"message",QString("Restoration could not finish: ")+QString::fromUtf8(e.what())}});throw;}
    auto status=readJson(dir+"/status.json");
    status["state"]="reverted";status["message"]="Previous settings restored from the saved snapshot.";
    writeJson(dir+"/status.json",status);
}
}
