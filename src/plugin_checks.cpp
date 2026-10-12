// SPDX-License-Identifier: MIT
#include "plugin_checks.h"
#include "core.h"
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QCryptographicHash>
#include <QFile>
#include <QSet>
#include <QFileInfo>
#include <QPluginLoader>
#include <QDirIterator>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QElapsedTimer>
#include <QThread>
#include <QTextStream>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <stdexcept>

namespace Studio {
static void blocked(const QString &detail) {
    throw std::runtime_error(("Plugin compatibility check blocked this operation: "+detail+
        " Finish a complete Qt/KDE update using your distribution's package manager, then log out and back in or restart the system. "
        "If it persists, repair/reinstall the named plugin and its Qt/KDE dependencies. "
        "Recovery restores settings, not system packages. Recovery snapshots have been preserved.").toStdString());
}
static void collectPlugins(const QJsonValue &value,QSet<QString> &result) {
    if(value.isArray()){for(const auto &v:value.toArray())collectPlugins(v,result);return;}
    if(!value.isObject())return;
    auto object=value.toObject();
    auto type=object["type"].toString();if(object.contains("id")&&!type.isEmpty())result.insert(type);
    // System-tray subapplets are recorded in recursive configuration nodes.
    auto plugin=object["entries"].toObject()["plugin"].toString();if(!plugin.isEmpty())result.insert(plugin);
    for(auto it=object.begin();it!=object.end();++it)if(it.value().isArray()||it.value().isObject())collectPlugins(it.value(),result);
}
QStringList recoveryPluginRequirements(const QJsonObject &snapshot) {
    QSet<QString> result;
    for(const auto &key:{"desktopLookBefore","desktopLooksBefore","removedPanelsBefore"})collectPlugins(snapshot[key],result);
    if(!snapshot["removedPanelsBefore"].toArray().isEmpty())result.insert("containment:org.kde.panel");
    for(const auto &entry:snapshot["wallpaperBefore"].toArray()){
        auto plugin=entry.toObject()["plugin"].toString();if(!plugin.isEmpty())result.insert("wallpaper:"+plugin);
    }
    if(snapshot["changeWallpaper"].toBool()&&snapshot["wallpaperBefore"].toArray().isEmpty()){
        auto plugin=snapshot["desktop"].toObject()["plugin"].toString();if(!plugin.isEmpty())result.insert("wallpaper:"+plugin);
    }
    auto list=result.values();list.sort();return list;
}
QStringList trialPluginRequirements(const QJsonObject &request,const QJsonObject &snapshot) {
    QSet<QString> result;
    for(const auto &plugin:recoveryPluginRequirements(snapshot))result.insert(plugin);
    for(const auto &plugin:request["widgets"].toArray())result.insert(plugin.toString());
    if(request["panelPopupActions"].toBool())result.insert("org.kde.plasma.icon");
    if(request["changeWallpaper"].toBool())result.insert("wallpaper:org.kde.image");
    if(!request["createPanels"].toArray().isEmpty())result.insert("containment:org.kde.panel");
    auto look=request["desktopLook"].toString();
    if(look.isEmpty()&&!request["createPanels"].toArray().isEmpty())look="plasma";
    if(!look.isEmpty()){
        result.insert(look=="plasma"?"org.kde.plasma.kickoff":"org.kamakiri.start");
        for(const auto &type:{"org.kde.plasma.panelspacer","org.kde.plasma.icontasks","org.kde.plasma.systemtray","org.kde.plasma.digitalclock"})result.insert(type);
        if(look=="fluent10"||look=="fluent11")for(const auto &type:{"org.kde.plasma.showdesktop","org.kde.plasma.notifications"})result.insert(type);
    }
    auto list=result.values();list.sort();return list;
}
QMap<QString,QString> plasmaLibraryIdentities(const QString &maps,bool verifyFiles) {
    QMap<QString,QString> libraries;
    const QRegularExpression line("^[0-9a-f]+-[0-9a-f]+\\s+\\S+\\s+[0-9a-f]+\\s+([0-9a-f]+):([0-9a-f]+)\\s+(\\d+)\\s+(.+)$");
    for(const auto &entry:maps.split('\n')){
        auto match=line.match(entry);if(!match.hasMatch())continue;
        QString path=match.captured(4);auto name=QFileInfo(path).fileName();
        if(!name.startsWith("libQt6")&&!name.startsWith("libKF6")&&!name.startsWith("libPlasma")&&!name.startsWith("libKirigami")&&!name.startsWith("libKScreen")&&!name.startsWith("libLayerShellQt")&&!path.contains("/qt6/qml/")&&!path.contains("/qt6/plugins/"))continue;
        if(path.endsWith(" (deleted)"))blocked("Plasma still has an older library loaded: "+name+". A session restart is required.");
        if(verifyFiles){struct stat info;auto bytes=QFile::encodeName(path);
            if(stat(bytes.constData(),&info)!=0||qulonglong(info.st_ino)!=match.captured(3).toULongLong()||major(info.st_dev)!=match.captured(1).toUInt(nullptr,16)||minor(info.st_dev)!=match.captured(2).toUInt(nullptr,16))
                blocked("The installed library differs from the one loaded by Plasma: "+name+".");
        }
        libraries[path]=match.captured(1)+":"+match.captured(2)+":"+match.captured(3);
    }
    return libraries;
}
void validatePlasmaRuntimeMaps(const QString &plasma,const QString &worker,bool verifyFiles) {
    auto shell=plasmaLibraryIdentities(plasma,verifyFiles),ours=plasmaLibraryIdentities(worker,verifyFiles);
    bool core=false;
    for(auto it=shell.begin();it!=shell.end();++it){
        auto name=QFileInfo(it.key()).fileName();
        if(name.startsWith("libQt6Core.so")){
            core=true;bool same=false;
            for(auto own=ours.begin();own!=ours.end();++own)if(QFileInfo(own.key()).fileName().startsWith("libQt6Core.so")&&own.key()==it.key()&&own.value()==it.value())same=true;
            if(!same)blocked("KamaKiriStudio and the running Plasma session use different Qt libraries.");
        }
        if(ours.contains(it.key())&&ours[it.key()]!=it.value())blocked("Qt/KDE library identities differ between the app and Plasma.");
    }
    if(!core)blocked("Cannot verify the running Plasma Qt library.");
}
static QString readPrivateProcessFile(const QString &path) {
    QFile file(path);if(!file.open(QIODevice::ReadOnly))blocked("Cannot inspect the running Plasma process. No compatibility assumptions will be made.");
    return QString::fromUtf8(file.readAll());
}
static QString sessionIdentity() {
    auto bus=QDBusConnection::sessionBus();
    auto reply=bus.interface()->servicePid("org.kde.plasmashell");
    if(!reply.isValid()||reply.value()==0)blocked("Cannot find the running Plasma session.");
    QString base="/proc/"+QString::number(reply.value());
    struct stat owner;if(stat(QFile::encodeName(base).constData(),&owner)!=0||owner.st_uid!=getuid())blocked("Cannot verify ownership of the Plasma process.");
    QString maps=readPrivateProcessFile(base+"/maps");
    validatePlasmaRuntimeMaps(maps,readPrivateProcessFile("/proc/self/maps"),true);
    auto environment=readPrivateProcessFile(base+"/environ").split(QChar('\0'));
    for(const auto &key:{"QT_PLUGIN_PATH","QML_IMPORT_PATH","QML2_IMPORT_PATH","XDG_DATA_DIRS"}){
        QString running;for(const auto &entry:environment)if(entry.startsWith(QString(key)+"="))running=entry.mid(QString(key).size()+1);
        if(running!=qEnvironmentVariable(key))blocked(QString("Plugin search environment differs from Plasma: ")+key+". Restart KamaKiriStudio from the Applications menu after restarting Plasma.");
    }
    QByteArray identities;auto libraries=plasmaLibraryIdentities(maps,true);
    for(auto it=libraries.begin();it!=libraries.end();++it)identities+=it.key().toUtf8()+it.value().toUtf8();
    return QString::number(reply.value())+":"+QString::fromLatin1(QCryptographicHash::hash(identities,QCryptographicHash::Sha256).toHex());
}
bool compilePluginQml(const QString &path,QString *problem) {
    QQmlEngine engine;const QUrl url=path.startsWith(":/")?QUrl("qrc"+path):QUrl::fromLocalFile(path);
    QQmlComponent component(&engine,url,QQmlComponent::PreferSynchronous);
    QElapsedTimer timer;timer.start();
    while(component.isLoading()&&timer.elapsed()<5000){QCoreApplication::processEvents();QThread::msleep(10);}
    if(component.status()!=QQmlComponent::Ready){QStringList errors;for(const auto &error:component.errors())errors.append(error.toString());*problem=errors.isEmpty()?"QML loading did not complete.":errors.join('\n');return false;}
    return true;
}
int probePlasmaPlugin(const QString &plugin) {
    QString id=plugin,folder="plasmoids";
    if(plugin.startsWith("wallpaper:")){id=plugin.mid(10);folder="wallpapers";}
    else if(plugin.startsWith("containment:")){id=plugin.mid(12);}
    if(!QRegularExpression("^[a-zA-Z0-9_.-]+$").match(id).hasMatch())return 2;
    // Some widget variants inherit another package/native plugin (for
    // example, Icons-only Task Manager inherits Task Manager).
    QSet<QString> visited;
    for(int depth=0;depth<8;depth++){
        if(visited.contains(id)){QTextStream(stderr)<<plugin<<": cyclic package inheritance\n";return 1;}visited.insert(id);
        auto metadataPath=QStandardPaths::locate(QStandardPaths::GenericDataLocation,"plasma/"+folder+"/"+id+"/metadata.json");
        if(metadataPath.isEmpty())break;
        auto root=readJson(metadataPath)["X-Plasma-RootPath"].toString();if(root.isEmpty())break;
        if(!QRegularExpression("^[a-zA-Z0-9_.-]+$").match(root).hasMatch()){QTextStream(stderr)<<plugin<<": invalid inherited package ID\n";return 1;}
        id=root;if(depth==7){QTextStream(stderr)<<plugin<<": package inheritance exceeds safe limit\n";return 1;}
    }
    auto path=QStandardPaths::locate(QStandardPaths::GenericDataLocation,"plasma/"+folder+"/"+id+"/contents/ui/main.qml");
    QPluginLoader native;QString nativePath;
    for(const auto &directory:QCoreApplication::libraryPaths()){
        auto candidate=directory+"/plasma/"+(folder=="wallpapers"?"wallpapers/":"applets/")+id+".so";
        if(QFileInfo::exists(candidate)){nativePath=candidate;break;}
    }
    QString problem;
    if(!nativePath.isEmpty()){
        native.setFileName(nativePath);
        if(!native.load()){QTextStream(stderr)<<plugin<<": "<<native.errorString()<<'\n';return 1;}
        if(path.isEmpty()){
            // Plasma 6.7+ embeds built-in QML in the native plugin's resources.
            // Resolve only a main.qml in this plugin's own module, never a
            // different widget's similarly named resource.
            QDirIterator resources(":/qt/qml",QDirIterator::Subdirectories);
            while(resources.hasNext()){
                auto candidate=resources.next();if(!candidate.endsWith("/main.qml"))continue;
                auto normalized=candidate;normalized.replace('/','.');normalized.replace('_','.');
                if(!normalized.contains("."+id+"."))continue;
                if(!path.isEmpty()){QTextStream(stderr)<<plugin<<": ambiguous embedded QML entry point\n";return 1;}
                path=candidate;
            }
        }
    }
    if(path.isEmpty())problem="The installed plugin's QML entry point is unavailable; compatibility cannot be verified.";
    else if(compilePluginQml(path,&problem)){QTextStream(stdout)<<"KAMAKIRI_PLUGIN_IMPORT_OK\n";return 0;}

    QTextStream(stderr)<<plugin<<": "<<problem<<'\n';return 1;
}
QString checkPluginSession(const QStringList &plugins) {
    // Isolated import probes use the running session's search environment and
    // identical on-disk Qt/KDE libraries. They do not instantiate widgets or
    // add temporary applets to the user's desktop.
    const auto before=sessionIdentity();
    for(const auto &plugin:plugins){
        QProcess process;auto environment=QProcessEnvironment::systemEnvironment();environment.insert("QML_DISABLE_DISK_CACHE","1");process.setProcessEnvironment(environment);
        process.start(QCoreApplication::applicationFilePath(),{"--probe-plugin",plugin});
        if(!process.waitForFinished(8000)){process.kill();process.waitForFinished();blocked(plugin+": plugin import probe timed out.");}
        if(process.exitStatus()!=QProcess::NormalExit||process.exitCode()!=0||!process.readAllStandardOutput().contains("KAMAKIRI_PLUGIN_IMPORT_OK"))blocked(plugin+": "+QString::fromUtf8(process.readAllStandardError()).left(1800));
    }
    if(before!=sessionIdentity())blocked("Plasma or its loaded libraries changed during the compatibility check. Retry after restarting the session.");
    return before;
}
}
