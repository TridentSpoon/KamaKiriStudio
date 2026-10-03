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
