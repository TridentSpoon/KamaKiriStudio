// SPDX-License-Identifier: MIT
#include "core.h"
#include "plugin_checks.h"
#include "desktop_frame.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QStandardPaths>
#include <QUuid>
#include <KConfig>
#include <KConfigGroup>

class CoreTest:public QObject {
    Q_OBJECT
private slots:
    void inlineStartIconUsesVerifiedConfigurationAndEscapedData(){
        const auto script=Studio::launcherIconScript(42,"view-grid");
        QVERIFY(script.contains("panelById(42)"));QVERIFY(script.contains("writeConfig('icon',"));QVERIFY(script.contains("readConfig('icon',"));QVERIFY(!script.contains("addWidget"));
        QVERIFY_EXCEPTION_THROWN(Studio::launcherIconScript(-1,"view-grid"),std::runtime_error);
        QJsonObject request{{"preset","caelestia"},{"accent","#509475"},{"startIcon","view-grid"}};Studio::validateRequest(request);
        request["startIcon"]="https://example.invalid/icon.svg";QVERIFY_EXCEPTION_THROWN(Studio::validateRequest(request),std::runtime_error);
    }

    void desktopFrameRespectsTheRailAndLeavesWindowSpaceClear() {
        auto path=Studio::desktopFramePath(QRectF(0,0,1280,800),{{"edge","left"},{"panelSize",42}});
        QVERIFY(!path.contains(QPointF(20,200))); // never paint over the existing rail
        QVERIFY(path.contains(QPointF(45,200))); // thin frame beside it
        QVERIFY(path.contains(QPointF(51,9))); // rounded inner corner
        QVERIFY(!path.contains(QPointF(400,400))); // transparent desktop/window area
        QVERIFY(!path.contains(QPointF(75,33))); // maximised window's reserved top-left
        Studio::validateDesktopFrame({});
        QJsonObject spec{{"name","Virtual-1"},{"edge","left"},{"panelSize",42}};
        QJsonObject config{{"version",1},{"color","#14231c"},{"screens",QJsonArray{spec}}};
        Studio::validateDesktopFrame(config);
        config["screens"]=QJsonArray{spec,spec};QVERIFY_EXCEPTION_THROWN(Studio::validateDesktopFrame(config),std::runtime_error);
        config["screens"]=QJsonArray{};QVERIFY_EXCEPTION_THROWN(Studio::validateDesktopFrame(config),std::runtime_error);
    }

    void oxygenGlowFollowsAccentAndRestoresOnlyItsOwnKeys() {
        QTemporaryDir directory;const auto path=directory.filePath("oxygenrc");
        {KConfig c(path,KConfig::SimpleConfig);auto g=c.group("ActiveShadow");g.writeEntry("InnerColor",QString("18,52,86"));g.writeEntry("UseAccentColor",false);g.writeEntry("ShadowSize",52);c.group("InactiveShadow").writeEntry("OuterColor",QString("32,32,32"));QVERIFY(c.sync());}
        const auto before=Studio::snapshotOxygenGlow(path);
        Studio::applyOxygenGlow(QColor("#509475"),path);
        {KConfig c(path,KConfig::SimpleConfig);auto g=c.group("ActiveShadow");QCOMPARE(g.readEntry("OuterColor",QString()),QString("80,148,117"));QVERIFY(g.readEntry("UseAccentColor",false));QCOMPARE(g.readEntry("ShadowSize",0),52);g.writeEntry("ShadowSize",60);QVERIFY(c.sync());}
        Studio::restoreOxygenGlow(before,path);QCOMPARE(Studio::snapshotOxygenGlow(path),before);
        {KConfig c(path,KConfig::SimpleConfig);QCOMPARE(c.group("ActiveShadow").readEntry("ShadowSize",0),60);QVERIFY(!c.group("ActiveShadow").hasKey("OuterColor"));QCOMPARE(c.group("InactiveShadow").readEntry("OuterColor",QString()),QString("32,32,32"));}
        QVERIFY_EXCEPTION_THROWN(Studio::applyOxygenGlow(QColor(),path),std::runtime_error);
    }

    void launcherSettingsOnlyOpenTheSelectedWidgetsConfiguration() {
        auto script=Studio::launcherSettingsScript(42);
        QVERIFY(script.contains("panelById(42)"));QVERIFY(script.contains("showConfigurationInterface()"));
        QVERIFY(script.contains("launchers.length>1"));QVERIFY(!script.contains("writeConfig"));QVERIFY(!script.contains("addWidget"));
        QVERIFY_EXCEPTION_THROWN(Studio::launcherSettingsScript(-1),std::runtime_error);
    }

    void personalWallpapersPersistAndStayWithTheirPreset() {
        QTemporaryDir source;QVERIFY(source.isValid());
        const auto oldData=qgetenv("XDG_DATA_HOME");qputenv("XDG_DATA_HOME",source.path().toUtf8());
        const QString preset="test-personal-"+QUuid::createUuid().toString(QUuid::Id128);
        const auto original=source.filePath("picture.png");QImage image(32,32,QImage::Format_RGB32);image.fill(Qt::red);QVERIFY(image.save(original));
        const auto saved=Studio::addThemeWallpaper(preset,original);
        QCOMPARE(Studio::addThemeWallpaper(preset,original),saved);
        QVERIFY(QFile::remove(original));QCOMPARE(Studio::themeWallpapers(preset),QStringList{saved});
        QVERIFY(!Studio::themeWallpapers("caelestia").contains(saved));
        QVERIFY_EXCEPTION_THROWN(Studio::addThemeWallpaper("../escape",saved),std::runtime_error);
        QFile invalid(source.filePath("invalid.png"));QVERIFY(invalid.open(QIODevice::WriteOnly));invalid.write("invalid");invalid.close();
        QVERIFY_EXCEPTION_THROWN(Studio::addThemeWallpaper(preset,invalid.fileName()),std::runtime_error);
        QVERIFY(QFile::remove(saved));
        if(oldData.isNull())qunsetenv("XDG_DATA_HOME");else qputenv("XDG_DATA_HOME",oldData);
    }

    void compatibilityIncludesNewLayoutAndRollbackPlugins() {
        QJsonObject icon{{"id",1},{"type","org.kde.plasma.icon"}};
        QJsonObject tray{{"id",2},{"type","org.kde.plasma.systemtray"},{"config",QJsonObject{{"entries",QJsonObject{{"plugin","org.kde.plasma.volume"}}}}}};
        QJsonObject snapshot{{"desktopLooksBefore",QJsonArray{QJsonObject{{"panel",QJsonObject{{"widgets",QJsonArray{icon,tray}}}}}}}};
        auto plugins=Studio::trialPluginRequirements(QJsonObject{{"desktopLook","fluent11"}},snapshot);
        QVERIFY(plugins.contains("org.kamakiri.start"));QVERIFY(plugins.contains("org.kde.plasma.icon"));
        QVERIFY(plugins.contains("org.kde.plasma.volume"));QVERIFY(plugins.contains("org.kde.plasma.showdesktop"));
        auto recovery=Studio::recoveryPluginRequirements(snapshot);
        QVERIFY(recovery.contains("org.kde.plasma.icon"));QVERIFY(!recovery.contains("org.kamakiri.start"));
        QVERIFY(Studio::trialPluginRequirements(QJsonObject{{"panelPopupActions",true}},{}).contains("org.kde.plasma.icon"));
        QVERIFY(Studio::trialPluginRequirements(QJsonObject{{"createPanels",QJsonArray{QJsonObject{{"screen",1}}}}},{}).contains("org.kde.plasma.kickoff"));
    }
    void compatibilityCoversRemovedPanelsAndLegacyWallpaperSnapshots() {
        QJsonObject snapshot{{"removedPanelsBefore",QJsonArray{QJsonObject{{"panel",QJsonObject{{"widgets",QJsonArray{QJsonObject{{"id",5},{"type","org.kde.plasma.kicker"}}}}}}}}},{"changeWallpaper",true},{"desktop",QJsonObject{{"plugin","org.kde.image"}}}};
        auto required=Studio::recoveryPluginRequirements(snapshot);
        QVERIFY(required.contains("org.kde.plasma.kicker"));QVERIFY(required.contains("containment:org.kde.panel"));QVERIFY(required.contains("wallpaper:org.kde.image"));
    }
    void runningPlasmaMustUseCurrentQtLibraries() {
        const QString core="1000-2000 r-xp 0000 08:01 100 /usr/lib/libQt6Core.so.6.11.2\n";
        Studio::validatePlasmaRuntimeMaps(core,core,false);
        QVERIFY_EXCEPTION_THROWN(Studio::validatePlasmaRuntimeMaps(core,QString(core).replace(" 100 "," 101 "),false),std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(Studio::validatePlasmaRuntimeMaps(QString(core).replace(".2\n",".2 (deleted)\n"),core,false),std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(Studio::validatePlasmaRuntimeMaps("",core,false),std::runtime_error);
    }
    void btrfsSubvolumeDevicesDoNotCauseFalseCompatibilityBlocks() {
        const QString mounts="1 0 0:33 /@ / rw - btrfs /dev/test rw\n2 1 8:1 / /media rw - ext4 /dev/other rw\n";
        QVERIFY(Studio::btrfsMappingDeviceMatches("/usr/lib/qt6/plugins/imageformats/kimg_xcf.so","0:33",mounts));
        QVERIFY(!Studio::btrfsMappingDeviceMatches("/usr/lib/test.so","0:99",mounts));
        QVERIFY(!Studio::btrfsMappingDeviceMatches("/media/test.so","0:33",mounts));
        QVERIFY(Studio::btrfsMappingDeviceMatches("/media-other/test.so","0:33",mounts));
        QVERIFY(!Studio::btrfsMappingDeviceMatches("/usr/lib/test.so","0:33",""));
        QVERIFY(Studio::btrfsMappingDeviceMatches("/path with space/test.so","0:44",mounts+"3 1 0:44 /@other /path\\040with\\040space rw - btrfs /dev/test rw\n"));
    }
    void changedKdeLibrariesAreDetectedBeforeMutation() {
        QTemporaryDir directory;QVERIFY(directory.isValid());
        QString path=directory.filePath("libQt6Core.so.6");QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write("fixture");file.close();
        const auto map=QString("1000-2000 r-xp 0000 00:00 999 %1\n").arg(path);
        QVERIFY_EXCEPTION_THROWN(Studio::plasmaLibraryIdentities(map,true),std::runtime_error);
    }
    void pluginProbeRejectsUnresolvedImportsWithoutCreatingObjects() {
        QTemporaryDir directory;QVERIFY(directory.isValid());QFile qml(directory.filePath("main.qml"));
        QVERIFY(qml.open(QIODevice::WriteOnly));qml.write("import QtQml\nQtObject { Component.onCompleted: { throw new Error('must not instantiate') } }\n");qml.close();
        QString problem;QVERIFY2(Studio::compilePluginQml(qml.fileName(),&problem),qPrintable(problem));
        QVERIFY(qml.open(QIODevice::WriteOnly|QIODevice::Truncate));qml.write("import Not.Installed.Plugin 99.0\nQtObject {}\n");qml.close();
        QVERIFY(!Studio::compilePluginQml(qml.fileName(),&problem));QVERIFY(problem.contains("Not.Installed.Plugin"));
    }

    void primaryMonitorFollowsKdePriorityRatherThanScreenOrder() {
        QJsonObject a{{"id",2},{"name","HDMI-A-1"},{"enabled",true},{"connected",true},{"priority",1},{"pos",QJsonObject{{"x",1200},{"y",1080}}}};
        auto wacom=a;wacom["id"]=3;wacom["name"]="DP-1";wacom["priority"]=2;wacom["pos"]=QJsonObject{{"x",0},{"y",224}};
        QJsonArray outputs{wacom,a};
        QCOMPARE(Studio::kdePrimaryConnector(outputs),QString("HDMI-A-1"));
        QJsonArray desktops{QJsonObject{{"id",9},{"screen",0},{"geometry",QJsonObject{{"x",0},{"y",224}}}},QJsonObject{{"id",10},{"screen",1},{"geometry",QJsonObject{{"x",1200},{"y",1080}}}}};
        QCOMPARE(Studio::kdePrimaryDesktopScreen(desktops,outputs),1);
        a["priority"]=2;wacom["priority"]=1;
        QCOMPARE(Studio::kdePrimaryConnector(QJsonArray{a,wacom}),QString("DP-1"));
        QCOMPARE(Studio::kdePrimaryDesktopScreen(desktops,QJsonArray{a,wacom}),0);
    }
    void primaryMonitorRejectsUnverifiableState() {
        QJsonObject output{{"id",1},{"name","DP-1"},{"connected",true},{"enabled",true},{"priority",1},{"pos",QJsonObject{{"x",0},{"y",0}}}};
        QVERIFY_EXCEPTION_THROWN(Studio::kdePrimaryConnector({}),std::runtime_error);
        output["enabled"]=false;
        QVERIFY_EXCEPTION_THROWN(Studio::kdePrimaryConnector(QJsonArray{output}),std::runtime_error);
        output["enabled"]=true;auto duplicate=output;duplicate["name"]="DP-2";
        QVERIFY_EXCEPTION_THROWN(Studio::kdePrimaryConnector(QJsonArray{output,duplicate}),std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(Studio::kdePrimaryDesktopScreen({},QJsonArray{output}),std::runtime_error);
        QJsonObject desktop{{"screen",0},{"geometry",QJsonObject{{"x",0},{"y",0}}}};auto mirror=desktop;mirror["screen"]=1;
        QVERIFY_EXCEPTION_THROWN(Studio::kdePrimaryDesktopScreen(QJsonArray{desktop,mirror},QJsonArray{output}),std::runtime_error);
    }
    void primaryPanelGuardReadsFreshKdeState() {
        QTemporaryDir temporary;QVERIFY(temporary.isValid());
        QFile doctor(temporary.filePath("kscreen-doctor"));QVERIFY(doctor.open(QIODevice::WriteOnly));
        doctor.write("#!/bin/sh\nprintf '%s' '{\"outputs\":[{\"id\":2,\"name\":\"HDMI-A-1\",\"connected\":true,\"enabled\":true,\"priority\":1,\"pos\":{\"x\":1200,\"y\":1080}}]}'\n");doctor.close();
        QVERIFY(doctor.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
        const auto path=qgetenv("PATH");qputenv("PATH",temporary.path().toUtf8()+":"+path);
        QJsonArray desktops{QJsonObject{{"screen",0},{"geometry",QJsonObject{{"x",0},{"y",0}}}},QJsonObject{{"screen",4},{"geometry",QJsonObject{{"x",1200},{"y",1080}}}}};
        int primary=Studio::primaryDesktopScreen(desktops);qputenv("PATH",path);
        QCOMPARE(primary,4);
    }

    void monitorNamesUseReportedModelAndConnector() {
        QCOMPARE(Studio::monitorDisplayName("Dell", "U2723QE", "DP-1"),QString("Dell U2723QE · DP-1"));
        QCOMPARE(Studio::monitorDisplayName("Dell", "DELL U2723QE", "DP-2"),QString("DELL U2723QE · DP-2"));
        QCOMPARE(Studio::monitorDisplayName("", "", "HDMI-A-1"),QString("HDMI-A-1"));
    }
    void primaryMonitorUsesFreshKdeOutputIdAndRejectsUnavailableDisplays() {
        QJsonObject output{{"id",42},{"name","DP-2"},{"enabled",true},{"connected",true}};
        QCOMPARE(Studio::primaryMonitorArgument(QJsonArray{output},"DP-2"),QString("output.42.priority.1"));
        QVERIFY_EXCEPTION_THROWN(Studio::primaryMonitorArgument(QJsonArray{output},"DP-1"),std::runtime_error);
        output["connected"]=false;
        QVERIFY_EXCEPTION_THROWN(Studio::primaryMonitorArgument(QJsonArray{output},"DP-2"),std::runtime_error);
        output["connected"]=true;output["enabled"]=false;
        QVERIFY_EXCEPTION_THROWN(Studio::primaryMonitorArgument(QJsonArray{output},"DP-2"),std::runtime_error);
        output["enabled"]=true;output["id"]=1.5;
        QVERIFY_EXCEPTION_THROWN(Studio::primaryMonitorArgument(QJsonArray{output},"DP-2"),std::runtime_error);
    }

    void panelSettingsComparisonIgnoresEmptyGroupsAndRetainsValues() {
        QJsonObject root{{"path",QJsonArray{}},{"entries",QJsonObject{{"popupWidth","560"}}},{"children",QJsonArray{}}};
        auto restored=root;restored["children"]=QJsonArray{QJsonObject{{"path",QJsonArray{"General"}},{"entries",QJsonObject{}},{"children",QJsonArray{}}}};
        QCOMPARE(Studio::comparablePanelWidgetConfig(root),Studio::comparablePanelWidgetConfig(restored));
        restored["entries"]=QJsonObject{{"popupWidth","400"}};
        QVERIFY(Studio::comparablePanelWidgetConfig(root)!=Studio::comparablePanelWidgetConfig(restored));
    }
    void configurationDialogGeometryDoesNotBlockRecovery() {
        QJsonObject original{{"path",QJsonArray{}},{"entries",QJsonObject{{"popupWidth","640"}}},{"children",QJsonArray{QJsonObject{{"path",QJsonArray{"General"}},{"entries",QJsonObject{{"look","fluent11"},{"favorites","dolphin.desktop"}}},{"children",QJsonArray{}}}}}};
        auto restored=original;auto children=restored["children"].toArray();children.append(QJsonObject{{"path",QJsonArray{"ConfigDialog"}},{"entries",QJsonObject{{"DialogHeight","630"},{"DialogWidth","810"}}},{"children",QJsonArray{}}});restored["children"]=children;
        QCOMPARE(Studio::comparablePanelWidgetConfig(original),Studio::comparablePanelWidgetConfig(restored));
        auto general=children[0].toObject();general["entries"]=QJsonObject{{"look","fluent10"},{"favorites","dolphin.desktop"}};children[0]=general;restored["children"]=children;
        QVERIFY(Studio::comparablePanelWidgetConfig(original)!=Studio::comparablePanelWidgetConfig(restored));
    }
    void mirroredIconCachePathDoesNotBlockRecovery() {
        QJsonObject general{{"path",QJsonArray{"General"}},{"entries",QJsonObject{{"url","file:///launcher.desktop"}}},{"children",QJsonArray{}}};
        QJsonObject original{{"path",QJsonArray{}},{"entries",QJsonObject{{"localPath","/cache/launcher.desktop"}}},{"children",QJsonArray{general}}};
        auto restored=original;auto entries=general["entries"].toObject();entries["localPath"]="/cache/launcher.desktop";general["entries"]=entries;restored["children"]=QJsonArray{general};
        QCOMPARE(Studio::comparablePanelWidgetConfig(original),Studio::comparablePanelWidgetConfig(restored));
        entries["url"]="file:///different.desktop";general["entries"]=entries;restored["children"]=QJsonArray{general};QVERIFY(Studio::comparablePanelWidgetConfig(original)!=Studio::comparablePanelWidgetConfig(restored));
    }
    void monitorSelectionExcludesDisabledDisplays() {
        QJsonObject a{{"id",1},{"screen",0},{"active",true},{"activity","a"},{"plugin","org.kde.image"}};
        auto b=a;b["id"]=2;b["screen"]=1;auto c=a;c["id"]=3;c["screen"]=2;
        auto result=Studio::wallpaperTargets(QJsonArray{a,b,c},1,"span",QJsonArray{2,3});
        QCOMPARE(result,QJsonArray({b,c}));
    }
    void panelRecoveryAcceptsLegacySnapshotsButVerifiesScreenWhenPresent() {
        QJsonObject old{{"id",4},{"location","bottom"},{"height",40},{"floating",true}};
        auto current=old;current["screen"]=1;QVERIFY(Studio::panelMatchesState(current,old));
        old["screen"]=0;QVERIFY(!Studio::panelMatchesState(current,old));
    }
    void legacySnapshotWidgetOrderUsesPositions() {
        QJsonObject a{{"id",97},{"index",2}}, b{{"id",172},{"index",0}}, c{{"id",173},{"index",1}};
        QCOMPARE(Studio::orderedPanelWidgets(QJsonArray{a,b,c}),QJsonArray({b,c,a}));
        QCOMPARE(Studio::orderedPanelWidgets(QJsonArray{c,a,b}),QJsonArray({b,c,a}));
    }
    void wallpaperCatalogContainsEveryThemeImage() {
        QStringList ids{"caelestia","ryoku","windows","breeze"};
        for(const auto &entry:Studio::omarchyPalettes())ids.append(entry.toObject()["id"].toString());
        for(const auto &id:ids){auto paths=Studio::themeWallpapers(id);QCOMPARE(paths.size(),id=="omarchy-retro-82"?8:id=="omarchy-tokyo-night"?7:id=="omarchy-catppuccin"?9:5);QCOMPARE(Studio::associatedWallpaper(id),paths.first());for(const auto &path:paths)QVERIFY(!Studio::loadWallpaper(path).isNull());}
        QVERIFY(Studio::themeWallpapers("missing-theme").isEmpty());
        for(const auto &id:{"omarchy-rose-pine","omarchy-lumon"})for(const auto &path:Studio::themeWallpapers(id)){QVERIFY(!path.contains("omarchy-plants"));QVERIFY(!path.contains("opinions-equally"));}

    }
    void wallpapersTargetConnectedCurrentActivityOnly() {
        QJsonObject one{{"id",1},{"screen",0},{"active",true},{"activity","current"},{"plugin","org.kde.image"}};
        auto two=one;two["id"]=2;two["screen"]=1;auto inactive=one;inactive["id"]=3;inactive["active"]=false;
        QJsonArray desktops{one,two,inactive};QCOMPARE(Studio::wallpaperTargets(desktops,1,"single").size(),1);QCOMPARE(Studio::wallpaperTargets(desktops,1,"all").size(),2);
        QVERIFY_EXCEPTION_THROWN(Studio::wallpaperTargets(desktops,3,"span"),std::runtime_error);
        two["plugin"]="unsupported";QVERIFY_EXCEPTION_THROWN(Studio::wallpaperTargets(QJsonArray{one,two},1,"all"),std::runtime_error);
    }
    void spanningUsesDisplayOffsetsAndBoundsMemory() {
        QImage source(96,24,QImage::Format_RGB32);source.fill(Qt::red);for(int x=48;x<96;x++)for(int y=0;y<24;y++)source.setPixelColor(x,y,Qt::blue);
        auto crops=Studio::spanWallpaper(source,{QRect(-48,0,48,24),QRect(0,0,48,24)});QCOMPARE(crops.size(),2);QCOMPARE(crops[0].pixelColor(20,12),QColor(Qt::red));QCOMPARE(crops[1].pixelColor(20,12),QColor(Qt::blue));
        QVERIFY_EXCEPTION_THROWN(Studio::spanWallpaper(source,{QRect(0,0,32000,32000)}),std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(Studio::spanWallpaper(source,{QRect()}),std::runtime_error);
        QImage portrait(1,100,QImage::Format_RGB32);portrait.fill(Qt::green);auto bounded=Studio::spanWallpaper(portrait,{QRect(0,0,1000,10)});QCOMPARE(bounded[0].size(),QSize(1000,10));
    }

    void desktopLooksRequireKnownLayoutAndPanel() {
        QJsonObject request{{"preset","windows"},{"accent","#0078d4"},{"desktopLook","fluent11"},{"changePanel",true},{"panel",QJsonObject{{"id",1},{"height",48},{"location","bottom"},{"floating",false}}}};
        Studio::validateRequest(request);request["panelPopupActions"]=true;QVERIFY_EXCEPTION_THROWN(Studio::validateRequest(request),std::runtime_error);request["panelPopupActions"]=false;request["desktopLook"]="'; bad()";QVERIFY_EXCEPTION_THROWN(Studio::validateRequest(request),std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(Studio::applyLookScript(1,"fluent11","bad","12345678-1234-1234-1234-123456789abc"),std::runtime_error);
        auto light=Studio::desktopPalette("omarchy-osaka-jade","light"),dark=Studio::desktopPalette("omarchy-osaka-jade","dark");
        QCOMPARE(light["accent"],dark["accent"]);QVERIFY(QColor(light["background"].toString()).lightnessF()>.8);QVERIFY(QColor(dark["background"].toString()).lightnessF()<.2);
    }

    void invalidPopupLookRefused() {
        QJsonObject request{{"preset","breeze"},{"accent","#6699cc"},{"changeColors",false},{"changePopupLook",true},{"popupStyle","fluent"},{"popupMode","light"}};
        Studio::validateRequest(request);request["popupMode"]="'; injected()";QVERIFY_EXCEPTION_THROWN(Studio::validateRequest(request),std::runtime_error);
    }

    void wallpaperChoicesAreLocalAndBounded() {
        QTemporaryDir tmp;QString path=tmp.filePath("test image;$(touch bad).png");
        QImage image(40,40,QImage::Format_RGB32);image.fill(QColor("#35b872"));QVERIFY(image.save(path));
        QVERIFY(!Studio::loadWallpaper(path).isNull());auto palette=Studio::wallpaperPalette(path);
        QVERIFY(QColor(palette["accent"].toString()).isValid());
        QVERIFY(QColor(palette["background"].toString()).lightnessF()<.15);
        QCOMPARE(Studio::wallpaperPalette(path),palette);
        QVERIFY_EXCEPTION_THROWN(Studio::loadWallpaper("https://example.com/image.png"),std::runtime_error);
        auto script=Studio::wallpaperScript(43,"file:///tmp/a'; throw new Error('x'); //");
        QVERIFY(script.contains(Studio::jsonString("file:///tmp/a'; throw new Error('x'); //")));
        QJsonObject r{{"preset","wallpaper"},{"accent",palette["accent"]},{"wallpaperPath",path},{"changeWallpaper",true},{"desktopId",43},{"widgets",QJsonArray{"org.kde.plasma.digitalclock"}}};
        Studio::validateRequest(r);r["desktopId"]=-1;QVERIFY_EXCEPTION_THROWN(Studio::validateRequest(r),std::runtime_error);
        r["desktopId"]=43;r["widgets"]=QJsonArray{"untrusted.plugin"};QVERIFY_EXCEPTION_THROWN(Studio::validateRequest(r),std::runtime_error);
    }
    void widgetRecoveryOnlyRemovesMarkedWidgets() {
        const QString owner="12345678-1234-1234-1234-123456789abc";
        auto add=Studio::addWidgetsScript(43,QJsonArray{"org.kde.plasma.digitalclock"},owner);
        QVERIFY(add.contains("d.widgets(type).length===0"));QVERIFY(add.contains("KamaKiriTransaction"));
        auto undo=Studio::removeTrialWidgetsScript(owner);QVERIFY(undo.contains("w.readConfig('KamaKiriTransaction','')==="+Studio::jsonString(owner)));
        QVERIFY_EXCEPTION_THROWN(Studio::removeTrialWidgetsScript("'; malicious()"),std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(Studio::addWidgetsScript(43,QJsonArray{"org.kde.plasma.digitalclock","org.kde.plasma.digitalclock"},owner),std::runtime_error);
    }

    void officialPalettesGenerateKdeSchemes() {
        auto rgb=[](const QColor &c){return QString("%1,%2,%3").arg(c.red()).arg(c.green()).arg(c.blue());};
        QCOMPARE(Studio::omarchyPalettes().size(),22);
        QTemporaryDir data; qputenv("XDG_DATA_HOME",data.path().toUtf8());
        for (const auto &value:Studio::omarchyPalettes()) {
            auto palette=value.toObject();
            for (const auto &key:{"accent","background","foreground","selection","dark_background","lighter_background","dark_foreground","red","green","orange","magenta"}) QVERIFY(QColor(palette[key].toString()).isValid());
            QJsonObject request{{"preset",palette["id"]},{"accent",palette["accent"]},{"light",false}};
            Studio::validateRequest(request);
            QString path=Studio::makeScheme(request,"12345678-1234-1234-1234-123456789abc");
            KConfig scheme(path,KConfig::SimpleConfig);
            QCOMPARE(scheme.group("Colors:Window").readEntry("BackgroundNormal",QString()),rgb(QColor(palette["background"].toString())));
            QCOMPARE(scheme.group("Colors:View").readEntry("ForegroundNormal",QString()),rgb(QColor(palette["foreground"].toString())));
            QCOMPARE(scheme.group("Colors:Selection").readEntry("BackgroundNormal",QString()),rgb(QColor(palette["selection"].toString())));
            QVERIFY(QFile::remove(path));
        }
        QCOMPARE(Studio::omarchyPalette("omarchy-osaka-jade")["accent"].toString(),QString("#509475"));
    }

    void noInputRevertsOnlyAtDeadline() {
        Studio::TrialPolicy p;
        QVERIFY(!p.shouldRevert(0,true));QVERIFY(!p.shouldRevert(14999,true));QVERIFY(p.shouldRevert(15000,true));
    }
    void firstInputRemovesDeadlinePermanently() {
        Studio::TrialPolicy p;p.input();
        QVERIFY(!p.shouldRevert(15000,true));QVERIFY(!p.shouldRevert(3600000,true));QVERIFY(!p.shouldRevert(43200000,true));
    }
    void noAndCrashRestoreAfterInput() {
        Studio::TrialPolicy p;p.input();QVERIFY(p.shouldRevert(700,false));p.rejected=true;QVERIFY(p.shouldRevert(700,true));
    }
    void yesKeeps() {Studio::TrialPolicy p;p.accepted=true;QVERIFY(!p.shouldRevert(15001,true));}
    void javascriptStringsAreData() {
        const QString raw="a\"'; throw new Error('injected'); //\n\\";
        QJsonDocument d=QJsonDocument::fromJson(("["+Studio::jsonString(raw)+"]").toUtf8());
        QCOMPARE(d.array()[0].toString(),raw);
    }
    void invalidPanelAndPresetRefused() {
        QJsonObject p{{"id",4},{"location","bottom"},{"height",44},{"floating",true}};
        QVERIFY(Studio::panelScript(p).contains("panelById(4)"));
        p["location"]="bottom'; run()";QVERIFY_EXCEPTION_THROWN(Studio::panelScript(p),std::runtime_error);
        QJsonObject r{{"preset","../../anything"},{"accent","#c4a7ff"}};
        QVERIFY_EXCEPTION_THROWN(Studio::validateRequest(r),std::runtime_error);
        r["preset"]="caelestia";r["accent"]="$(id)";QVERIFY_EXCEPTION_THROWN(Studio::validateRequest(r),std::runtime_error);
    }
    void colorRestorePreservesUnrelatedPreferences() {
        QTemporaryDir tmp;QString before=tmp.filePath("before"),now=tmp.filePath("now");
        {
            KConfig b(before,KConfig::SimpleConfig);
            b.group("General").writeEntry("ColorScheme","Old");
            b.group("Colors:Window").writeEntry("BackgroundNormal","1,2,3");
            b.group("Colors:Header").group("Inactive").writeEntry("BackgroundNormal","9,8,7");
            b.group("KDE").writeEntry("contrast",3);QVERIFY(b.sync());
            KConfig n(now,KConfig::SimpleConfig);
            n.group("General").writeEntry("ColorScheme","New");n.group("General").writeEntry("AccentColor","5,6,7");
            n.group("General").writeEntry("Font","Keep this font");
            n.group("KDE").writeEntry("widgetStyle","KeepThisStyle");
            n.group("Colors:Window").writeEntry("BackgroundNormal","4,5,6");
            n.group("Colors:Header").group("Inactive").writeEntry("BackgroundNormal","6,5,4");
            n.group("Colors:View").writeEntry("ForegroundNormal","255,255,255");
            n.group("KDE").writeEntry("contrast",7);QVERIFY(n.sync());
        }
        Studio::restoreColors(before,now);
        KConfig n(now,KConfig::SimpleConfig);
        QCOMPARE(n.group("General").readEntry("ColorScheme",QString()),QString("Old"));
        QVERIFY(!n.group("General").hasKey("AccentColor"));
        QCOMPARE(n.group("General").readEntry("Font",QString()),QString("Keep this font"));
        QCOMPARE(n.group("KDE").readEntry("widgetStyle",QString()),QString("KeepThisStyle"));
        QCOMPARE(n.group("Colors:Window").readEntry("BackgroundNormal",QString()),QString("1,2,3"));
        QCOMPARE(n.group("Colors:Header").group("Inactive").readEntry("BackgroundNormal",QString()),QString("9,8,7"));
        QVERIFY(!n.hasGroup("Colors:View"));QCOMPARE(n.group("KDE").readEntry("contrast",0),3);
    }
    void sessionsDoNotExecuteDesktopCommands() {
        QTemporaryDir tmp;QString root=tmp.filePath("wayland-sessions");QDir().mkpath(root);
        QString marker=tmp.filePath("executed"),file=root+"/test.desktop";
        QFile f(file);QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(("[Desktop Entry]\nName=Harmless test\nExec=touch "+marker+"\nTryExec=/missing/command\n").toUtf8());f.close();
        auto s=Studio::sessions({root});QCOMPARE(s.size(),1);QVERIFY(!s[0].toObject()["available"].toBool());QVERIFY(!QFileInfo::exists(marker));
    }
    void jsonRecordIsPrivateAndAtomic() {
        QTemporaryDir tmp;QString path=tmp.filePath("record.json");
        Studio::writeJson(path,{{"a",1}});Studio::writeJson(path,{{"b",2}});
        auto o=Studio::readJson(path);QVERIFY(!o.contains("a"));QCOMPARE(o["b"].toInt(),2);
        QVERIFY(!(QFileInfo(path).permissions()&(QFile::ReadGroup|QFile::ReadOther|QFile::WriteGroup|QFile::WriteOther)));
    }
};
QTEST_MAIN(CoreTest)
#include "core_test.moc"
