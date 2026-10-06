// SPDX-License-Identifier: MIT
#include "core.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <KConfig>
#include <KConfigGroup>

class CoreTest:public QObject {
    Q_OBJECT
private slots:
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
        for(const auto &id:ids){auto paths=Studio::themeWallpapers(id);QCOMPARE(paths.size(),id=="omarchy-retro-82"?8:id=="omarchy-tokyo-night"?7:5);QCOMPARE(Studio::associatedWallpaper(id),paths.first());for(const auto &path:paths)QVERIFY(!Studio::loadWallpaper(path).isNull());}
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
