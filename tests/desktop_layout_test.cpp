// SPDX-License-Identifier: MIT
#include "desktop_layout.h"
#include <QtTest>
#include <QQuickItem>
#include <QTemporaryDir>
class DesktopLayoutTest:public QObject {
    Q_OBJECT
private slots:
    void roundedFrameUsesSelectedWallpaperAndResizes() {
        QTemporaryDir directory;
        QImage wallpaper(40,40,QImage::Format_RGB32);wallpaper.fill(Qt::red);
        const auto path=directory.filePath("wallpaper.png");QVERIFY(wallpaper.save(path));
        QScopedPointer<QQuickWidget> view(desktopLayoutPreview(path,{{"background","#123456"},{"foreground","#ffffff"},{"accent","#abcdef"}}));
        view->setAttribute(Qt::WA_DeleteOnClose,false);view->resize(800,500);view->show();
        auto root=view->rootObject();QVERIFY(root);root->setProperty("showControls",false);
        QTest::qWait(120);
        const auto image=view->grab().toImage();const auto dpr=image.devicePixelRatio();
        auto pixel=[&](int x,int y){return image.pixelColor(qRound(x*dpr),qRound(y*dpr));};
        QCOMPARE(pixel(10,200),QColor("#123456")); // left rail
        QCOMPARE(pixel(300,2),QColor("#123456")); // top frame
        QCOMPARE(pixel(43,9),QColor("#123456")); // rounded inner corner
        QCOMPARE(pixel(300,200),QColor(Qt::red)); // wallpaper
        QCOMPARE(root->property("viewport").toRectF(),QRectF(42,8,750,484));
        root->setProperty("railWidth",60);root->setProperty("frameWidth",12);view->resize(1000,600);QTest::qWait(60);
        QCOMPARE(root->property("viewport").toRectF(),QRectF(60,12,928,576));
        QTest::keyClick(view.data(),Qt::Key_Escape);QVERIFY(root->property("showControls").toBool());
    }
};
QTEST_MAIN(DesktopLayoutTest)
#include "desktop_layout_test.moc"
